#include "mkrecv_storage_single_buffer.h"

namespace mkrecv
{

  storage_single_buffer::storage_single_buffer(std::shared_ptr<mkrecv::options> hopts, bool alloc_data) :
    storage(hopts)
  {
    std::size_t  data_size  = MAX_DATA_SPACE;
    std::size_t  temp_size  = MAX_TEMPORARY_SPACE;
    std::size_t  trash_size = MAX_TEMPORARY_SPACE;

    if (heap_size != HEAP_SIZE_DEF)
      {
	std::size_t group_size;
	group_size = heap_count*(heap_size + nsci*sizeof(spead2::s_item_pointer_t));
	data_size  = opts->ngroups_data*group_size; // N heap groups (N = option NGROUPS_DATA)
	temp_size  = opts->ngroups_temp*group_size; // N heap groups (N = option NGROUPS_TEMP)
	trash_size =                    group_size; // 1 heap group
      }
    if (alloc_data)
      {
	dest[DATA_DEST].allocate_buffer(memallocator, data_size);
	std::cout << "dest[DATA_DEST].ptr.ptr()  = " << (std::size_t)(dest[DATA_DEST].ptr->ptr()) << '\n';
      }
    dest[TEMP_DEST].allocate_buffer(memallocator, temp_size);
    std::cout << "dest[TEMP_DEST].ptr.ptr()  = " << (std::size_t)(dest[TEMP_DEST].ptr->ptr()) << '\n';
    dest[TRASH_DEST].allocate_buffer(memallocator, trash_size);
    std::cout << "dest[TRASH_DEST].ptr.ptr() = " << (std::size_t)(dest[TRASH_DEST].ptr->ptr()) << '\n';
  }

  storage_single_buffer::~storage_single_buffer()
  {
  }
  
  int storage_single_buffer::handle_alloc_place(spead2::s_item_pointer_t &group_index, int dest_index)
  {
    spead2::s_item_pointer_t timestamp = timestamp_first + group_index*timestamp_step;
    
    // This is a normal heap which _should_ go into a ringbuffer if nothing else is wrong.
    if (group_index < 0)
      {
	// The timestamp is smaller than the timestamp of the first heap in the current slot
	// -> put this heap into trash and report it as a skipped heap
	gstat.heaps_skipped++;
	//std::cout << "TS too old: " << timestamp << " " << timestamp_first << " " << timestamp_step << " -> " << group_index << '\n';
	dest_index = TRASH_DEST;
      }
    else if (state == SEQUENTIAL_STATE)
      {
	if (group_index >= (spead2::s_item_pointer_t)(dest[DATA_DEST].capacity + dest[TEMP_DEST].capacity))
	  {
	    gstat.heaps_overrun++;
	    std::cout << "SEQ overrun: " << timestamp << " " << timestamp_first << " " << timestamp_step << " -> " << group_index << '\n';
	    dest_index = TRASH_DEST;
	  }
	else if (group_index >= (spead2::s_item_pointer_t)dest[DATA_DEST].capacity)
	  {
	    dest_index = TEMP_DEST;
	    group_index -= (spead2::s_item_pointer_t)dest[DATA_DEST].capacity;
	  }
      }
    else if (state == PARALLEL_STATE)
      {
	if (group_index >= (spead2::s_item_pointer_t)dest[DATA_DEST].capacity)
	  {
	    gstat.heaps_overrun++;
	    //std::cout << "PAR overrun: " << timestamp << " " << timestamp_first << " " << timestamp_step << " -> " << group_index << '\n';
	    dest_index = TRASH_DEST;
	  }
	else if (group_index < (spead2::s_item_pointer_t)dest[TEMP_DEST].capacity)
	  {
	    dest_index = TEMP_DEST;
	  }
      }
    return dest_index;
  }
  
  void storage_single_buffer::do_switch_slot()
  {
  }
  
  void storage_single_buffer::do_release_slot()
  {
  }
  
  void storage_single_buffer::do_copy_temp()
  {
  }
  
  void storage_single_buffer::handle_free_place(spead2::s_item_pointer_t timestamp, int dest_index)
  {
    (void)dest_index;
    
    if (dest[DATA_DEST].needed > dest[DATA_DEST].space)
      {
        //std::cout << "warning needed < 0 state " << state << " needed " << dest[DATA_DEST].needed << " heaps_open " << dstat[DATA_DEST].heaps_open << "," << dstat[TEMP_DEST].heaps_open << '\n';
      }
    //std::cout << "mark " << cnt << " isok " << isok << " dest " << d << " needed " << nd << " " << nt << " cts " << ctsd << " " << ctst << '\n';
    if ((state == SEQUENTIAL_STATE) && (timestamp >= timestamp_temp_level))
      {
	// switch to parallel data/temp order
	std::cout << "STAT "
		  << dest[DATA_DEST].space << " "
		  << dstat[DATA_DEST].heaps_completed << " "
		  << dstat[DATA_DEST].heaps_discarded << " "
		  << dest[DATA_DEST].needed << " "
		  << dstat[DATA_DEST].bytes_expected << " "
		  << dstat[DATA_DEST].bytes_received << " "
                  << gstat.heaps_completed << " "
                  << gstat.heaps_discarded << " "
                  << gstat.heaps_needed << " "
                  << gstat.bytes_expected << " "
                  << gstat.bytes_received
		  << "\n";
        gstat.heaps_needed += dest[DATA_DEST].needed;
	//std::cout << "still needing " << dest[DATA_DEST].needed << " heaps." << '\n';
	state = PARALLEL_STATE;
	if (!has_stopped)
	  { // copy the optional side-channel items at the correct position
	    // sci_base = buffer + size - (scape *nsci)
	    do_switch_slot();
	  }
	if (stop && !has_stopped)
	  {
	    has_stopped = true;
	    std::cout << "request to stop the transfer into the ringbuffer received." << '\n';
	    do_release_slot();
	  }
	dest[DATA_DEST].needed  = dest[DATA_DEST].space;
	timestamp_first  += dest[DATA_DEST].capacity*timestamp_step;
	timestamp_data_level = timestamp_first + timestamp_data_count*timestamp_step;
	dstat[DATA_DEST].heaps_completed = 0;
	dstat[DATA_DEST].heaps_discarded = 0;
	dstat[DATA_DEST].bytes_expected = 0;
	dstat[DATA_DEST].bytes_received = 0;
	if (!opts->quiet)
	  {
	    std::cout << "-> parallel "; show_log();
	  }
      }
    else if ((state == PARALLEL_STATE) && (timestamp >= timestamp_data_level))
      {
	// switch to sequential data/temp order
	state = SEQUENTIAL_STATE;
	if (!has_stopped)
	  { // copy the heaps in temporary space into data space
	    do_copy_temp();
	    dstat[DATA_DEST].heaps_completed += dstat[TEMP_DEST].heaps_completed;
	    dstat[DATA_DEST].heaps_discarded += dstat[TEMP_DEST].heaps_discarded;
	    dstat[DATA_DEST].bytes_expected += dstat[TEMP_DEST].bytes_expected;
	    dstat[DATA_DEST].bytes_received += dstat[TEMP_DEST].bytes_received;
	  }
	timestamp_temp_level = timestamp_first + timestamp_temp_count*timestamp_step;
	dest[DATA_DEST].needed -= (dest[TEMP_DEST].space - dest[TEMP_DEST].needed);
        if (dest[DATA_DEST].needed > dest[DATA_DEST].space)
          {
            //std::cout << "warning, p -> s, needed < 0 " << dest[DATA_DEST].needed << " heaps_open " << dstat[DATA_DEST].heaps_open << "," << dstat[TEMP_DEST].heaps_open << '\n';
          }
        dest[TEMP_DEST].needed  = dest[TEMP_DEST].space;
	dstat[TEMP_DEST].heaps_completed = 0;
	dstat[TEMP_DEST].heaps_discarded = 0;
	dstat[TEMP_DEST].bytes_expected = 0;
	dstat[TEMP_DEST].bytes_received = 0;
	if (!opts->quiet)
	  {
	    std::cout << "-> sequential "; show_log();
	  }
      }
  }

  void storage_single_buffer::request_stop()
  {
    stop = true;
  }

  bool storage_single_buffer::is_stopped()
  {
    return has_stopped;
  }

  void storage_single_buffer::close()
  {
  }


}
