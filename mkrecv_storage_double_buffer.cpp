#include "mkrecv_storage_double_buffer.h"

namespace mkrecv
{

  storage_double_buffer::storage_double_buffer(std::shared_ptr<mkrecv::options> hopts) :
    storage(hopts)
  {
    std::size_t  data_size  = MAX_DATA_SPACE;
    std::size_t  trash_size = MAX_TEMPORARY_SPACE;

    if (heap_size != HEAP_SIZE_DEF)
      {
	std::size_t group_size;
	group_size = heap_count*(heap_size + nsci*sizeof(spead2::s_item_pointer_t));
	data_size  = opts->ngroups_data*group_size; // N heap groups (N = option NGROUPS_DATA)
	trash_size =                    group_size; // 1 heap group
      }
    dest[DATA_DEST].allocate_buffer(memallocator, data_size);
    std::cout << "dest[DATA_DEST].ptr.ptr()  = " << (std::size_t)(dest[DATA_DEST].ptr->ptr()) << std::endl;
    dest[TEMP_DEST].allocate_buffer(memallocator, data_size);
    std::cout << "dest[TEMP_DEST].ptr.ptr()  = " << (std::size_t)(dest[TEMP_DEST].ptr->ptr()) << std::endl;
    dest[TRASH_DEST].allocate_buffer(memallocator, trash_size);
    std::cout << "dest[TRASH_DEST].ptr.ptr() = " << (std::size_t)(dest[TRASH_DEST].ptr->ptr()) << std::endl;
  }

  storage_double_buffer::~storage_double_buffer()
  {
  }

  int storage_double_buffer::handle_alloc_place(spead2::s_item_pointer_t &group_index, int dest_index)
  {
    spead2::s_item_pointer_t timestamp = timestamp_first + group_index*timestamp_step;
    int                      tindex = (dindex + 1)%2;

    // This is a normal heap which _should_ go into a ringbuffer if nothing else is wrong.
    if (group_index < 0)
      {
	// The timestamp is smaller than the timestamp of the first heap in the current slot
	// -> put this heap into trash and report it as a skipped heap
	gstat.heaps_skipped++;
	std::cout << "TS too old: " << timestamp << " " << timestamp_first << " " << timestamp_step << " -> " << group_index << std::endl;
	dest_index = TRASH_DEST;
      }
    else
      {
	if (group_index >= (spead2::s_item_pointer_t)(dest[dindex].capacity + dest[tindex].capacity))
	  {
	    gstat.heaps_overrun++;
	    std::cout << "SEQ overrun: " << timestamp << " " << timestamp_first << " " << timestamp_step << " -> " << group_index << std::endl;
	    dest_index = TRASH_DEST;
	  }
	else if (group_index >= (spead2::s_item_pointer_t)dest[dindex].capacity)
	  {
	    dest_index = TEMP_DEST;
	    group_index -= (spead2::s_item_pointer_t)dest[dindex].capacity;
	  }
      }
    if (dest_index != TRASH_DEST) 
      {
	dest_index = (dindex + dest_index)%2; // DATA_DEST -> dindex, TEMP_DEST -> tindex
      }
    return dest_index;
  }


  
  void storage_double_buffer::do_switch_slot()
  {
  }
  
  void storage_double_buffer::do_release_slot()
  {
  }
  
  void storage_double_buffer::do_copy_temp()
  {
  }
  
  void storage_double_buffer::handle_free_place(spead2::s_item_pointer_t timestamp, int dest_index)
  {
    (void)dest_index;
    int                 tindex;

    if (dest[DATA_DEST].needed > dest[DATA_DEST].space)
      {
        //std::cout << "warning needed < 0 state " << state << " needed " << dest[DATA_DEST].needed << " heaps_open " << dstat[DATA_DEST].heaps_open << "," << dstat[TEMP_DEST].heaps_open << std::endl;
      }
    //std::cout << "mark " << cnt << " isok " << isok << " dest " << d << " needed " << nd << " " << nt << " cts " << ctsd << " " << ctst << std::endl;
    if (timestamp >= timestamp_temp_level)
      {
	// switch to next buffer
        std::cout << "still needing " << dest[dindex].needed << " heaps." << std::endl;
        gstat.heaps_needed += dest[dindex].needed;
	if (!has_stopped)
	  { // copy the optional side-channel items at the correct position
	    // sci_base = buffer + size - (scape *nsci)
	    do_switch_slot();
            dindex = (dindex + 1)%2;
	  }
	if (stop && !has_stopped)
	  {
	    has_stopped = true;
	    std::cout << "request to stop the transfer into the ringbuffer received." << std::endl;
	    do_release_slot();
	  }
        tindex = (dindex + 1)%2;
	dest[tindex].needed  = dest[tindex].space;
	timestamp_first  += dest[dindex].capacity*timestamp_step;
	timestamp_temp_level = timestamp_first + timestamp_data_count*timestamp_step;
	std::cout << "-> sequential "; show_log();
      }
  }

  void storage_double_buffer::request_stop()
  {
    stop = true;
  }

  bool storage_double_buffer::is_stopped()
  {
    return has_stopped;
  }

  void storage_double_buffer::close()
  {
  }


}