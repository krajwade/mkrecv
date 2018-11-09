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
      }
    dest[TEMP_DEST].allocate_buffer(memallocator, temp_size);
    dest[TRASH_DEST].allocate_buffer(memallocator, trash_size);
    if (alloc_data)
      {
	std::cout << "dest[DATA_DEST].ptr.ptr()  = " << (std::size_t)(dest[DATA_DEST].ptr->ptr()) << std::endl;
      }
    std::cout << "dest[TEMP_DEST].ptr.ptr()  = " << (std::size_t)(dest[TEMP_DEST].ptr->ptr()) << std::endl;
    std::cout << "dest[TRASH_DEST].ptr.ptr() = " << (std::size_t)(dest[TRASH_DEST].ptr->ptr()) << std::endl;
  }

  storage_single_buffer::~storage_single_buffer()
  {
  }

  void storage_single_buffer::do_init(spead2::s_item_pointer_t timestamp,     // timestamp of a heap
			     std::size_t size                        // heap size (only payload)
			     )
  {
    if (heap_size == HEAP_SIZE_DEF) heap_size = size;
    timestamp_first = timestamp + 2*timestamp_step; // 2 is a safety margin to avoid incomplete heaps
    dest[DATA_DEST].set_heap_size(heap_size, heap_count, 0, nsci);
    dest[TEMP_DEST].set_heap_size(heap_size, heap_count, 0, nsci);
    dest[TRASH_DEST].set_heap_size(heap_size, heap_count, 0, nsci);
    level_data_count = ((dest[DATA_DEST].capacity - dest[TEMP_DEST].capacity)*opts->level_data)/100 + dest[TEMP_DEST].capacity;
    level_temp_count = (dest[TEMP_DEST].capacity*opts->level_temp)/100 + dest[DATA_DEST].capacity;
    state = SEQUENTIAL_STATE;
    // timestemp limit for SEQUENTIAL -> PARALLEL switch
    timestamp_level_temp = timestamp_first + level_temp_count*timestamp_step;
    std::cout << "sizes: heap size " << heap_size << " count " << heap_count << " first " << timestamp_first << " step " << timestamp_step << std::endl;
    std::cout << "DATA_DEST:  capacity " << dest[DATA_DEST].capacity << " space " << dest[DATA_DEST].space << std::endl;
    std::cout << "TEMP_DEST:  capacity " << dest[TEMP_DEST].capacity << " space " << dest[TEMP_DEST].space << std::endl;
    std::cout << "TRASH_DEST: capacity " << dest[TRASH_DEST].capacity << " space " << dest[TRASH_DEST].space << std::endl;
    //std::cout << "cts " << dest[DATA_DEST].cts << " " << dest[TEMP_DEST].cts << " " << dest[TRASH_DEST].cts << std::endl;
    std::cout << "level_data_count " << level_data_count << " level_temp_count " << level_temp_count << " timestamp_level_temp " << timestamp_level_temp << std::endl;
  }
  
  int storage_single_buffer::alloc_place(spead2::s_item_pointer_t timestamp,     // timestamp of a heap
				std::size_t heap_index,                 // heap number inside a heap group
				std::size_t size,                       // heap size (only payload)
				int dest_index,                         // requested destination (DATA_DEST or TRASH_DEST)
				char *&heap_place,                      // returned memory pointer to this heap payload
				spead2::s_item_pointer_t *&sci_place)   // returned memory pointer to the side-channel items for this heap
  {
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
    char                           *mem_base = NULL;
    spead2::s_item_pointer_t        mem_offset;
    spead2::s_item_pointer_t       *sci_base = NULL;
    spead2::s_item_pointer_t        sci_offset;
    spead2::s_item_pointer_t        group_index;

    // **** GUARDED BY SEMAPHORE ****
    std::lock_guard<std::mutex> lock(dest_mutex);
    if ((state == INIT_STATE) && (dest_index == DATA_DEST))
      {
	do_init(timestamp, size);
      }
    if (stop)
      {
	dest_index = TRASH_DEST;
	has_stopped = true;
      }
    if (dest_index == DATA_DEST)
      {
	group_index = (timestamp - timestamp_first)/timestamp_step;
	// This is a normal heap which _should_ go into a ringbuffer if nothing else is wrong.
 	if (group_index < 0)
	  {
	    // The timestamp is smaller than the timestamp of the first heap in the current slot
	    // -> put this heap into trash and report it as a skipped heap
	    gstat.heaps_skipped++;
	    std::cout << "TS too old: " << timestamp << " " << timestamp_first << " " << timestamp_step << " -> " << group_index << std::endl;
	    dest_index = TRASH_DEST;
	  }
        else if (state == SEQUENTIAL_STATE)
          {
	    if (group_index >= (spead2::s_item_pointer_t)(dest[DATA_DEST].capacity + dest[TEMP_DEST].capacity))
	      {
	        gstat.heaps_overrun++;
	        std::cout << "SEQ overrun: " << timestamp << " " << timestamp_first << " " << timestamp_step << " -> " << group_index << std::endl;
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
		//std::cout << "PAR overrun: " << timestamp << " " << timestamp_first << " " << timestamp_step << " -> " << group_index << std::endl;
                dest_index = TRASH_DEST;
              }
           else if (group_index < (spead2::s_item_pointer_t)dest[TEMP_DEST].capacity)
              {
                dest_index = TEMP_DEST;
              }
          }
      }
    else
      { // The requested destination is already TRASH_DEST -> This heap is should be ignored
	gstat.heaps_ignored++;
      }
    if (dest_index == TRASH_DEST) group_index = 0;
    dest[dest_index].count++;
    gstat.heaps_total++;
    gstat.heaps_open++;
    gstat.bytes_expected += size;
    dstat[dest_index].heaps_total++;
    dstat[dest_index].heaps_open++;
    dstat[dest_index].bytes_expected += size;
    mem_base = dest[dest_index].ptr->ptr();
    sci_base = dest[dest_index].sci;
    mem_offset = heap_size*(group_index*heap_count + heap_index);
    sci_offset = group_index*heap_count + heap_index;
    heap_place = mem_base + mem_offset;
    sci_place  = sci_base + sci_offset;
    if (gstat.heaps_total == 10*dest[DATA_DEST].size) et.reset();
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    et.add_et(et_statistics::ALLOC_TIMING, std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count());
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
  
  void storage_single_buffer::free_place(spead2::s_item_pointer_t timestamp,    // timestamp of a heap
				int dest_index,                        // destination of a heap
				std::size_t reclen)                    // recieved number of bytes
  {
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
    //std::size_t  ctsd, ctst;

    // **** GUARDED BY SEMAPHORE ****
    std::lock_guard<std::mutex> lock(dest_mutex);
    gstat.heaps_open--;
    gstat.bytes_received += reclen;
    dstat[dest_index].heaps_open--;
    dstat[dest_index].bytes_received += reclen;
    if (reclen == heap_size)
      {
	gstat.heaps_completed++;
	dstat[dest_index].heaps_completed++;
      }
    else
      {
	gstat.heaps_discarded++;
	dstat[dest_index].heaps_discarded++;
      }
    dest[dest_index].needed--;
    if (dest[DATA_DEST].needed > dest[DATA_DEST].space)
      {
        //std::cout << "warning needed < 0 state " << state << " needed " << dest[DATA_DEST].needed << " heaps_open " << dstat[DATA_DEST].heaps_open << "," << dstat[TEMP_DEST].heaps_open << std::endl;
      }
    //show_mark_log();
    //std::cout << "mark " << cnt << " isok " << isok << " dest " << d << " needed " << nd << " " << nt << " cts " << ctsd << " " << ctst << std::endl;
    if ((state == SEQUENTIAL_STATE) && (timestamp >= timestamp_level_temp))
      {
	// switch to parallel data/temp order
        std::cout << "still needing " << dest[DATA_DEST].needed << " heaps." << std::endl;
	state = PARALLEL_STATE;
	if (!has_stopped)
	  { // copy the optional side-channel items at the correct position
	    // sci_base = buffer + size - (scape *nsci)
	    do_switch_slot();
	  }
	if (stop && !has_stopped)
	  {
	    has_stopped = true;
	    std::cout << "request to stop the transfer into the ringbuffer received." << std::endl;
	    do_release_slot();
	  }
	dest[DATA_DEST].needed  = dest[DATA_DEST].space;
	timestamp_first  += dest[DATA_DEST].capacity*timestamp_step;
	timestamp_level_data = timestamp_first + level_data_count*timestamp_step;
	show_state_log();
      }
    else if ((state == PARALLEL_STATE) && (timestamp >= timestamp_level_data))
      {
	// switch to sequential data/temp order
	state = SEQUENTIAL_STATE;
	if (!has_stopped)
	  { // copy the heaps in temporary space into data space
	    do_copy_temp();
	  }
	timestamp_level_temp = timestamp_first + level_temp_count*timestamp_step;
	dest[DATA_DEST].needed -= (dest[TEMP_DEST].space - dest[TEMP_DEST].needed);
        if (dest[DATA_DEST].needed > dest[DATA_DEST].space)
          {
            //std::cout << "warning, p -> s, needed < 0 " << dest[DATA_DEST].needed << " heaps_open " << dstat[DATA_DEST].heaps_open << "," << dstat[TEMP_DEST].heaps_open << std::endl;
          }
        dest[TEMP_DEST].needed  = dest[TEMP_DEST].space;
	//dest[TEMP_DEST].cts = cts_temp;
	show_state_log();
      }
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    et.add_et(et_statistics::MARK_TIMING, std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count());
  }

  void storage_single_buffer::show_mark_log()
  {
    if ((gstat.heaps_total - log_counter) >= LOG_FREQ)
      {
	log_counter += LOG_FREQ;
	std::cout << "heaps:"
		  << " total "
		  << gstat.heaps_total
		  << "=" << dstat[DATA_DEST].heaps_total
		  << "+" << dstat[TEMP_DEST].heaps_total
		  << "+" << dstat[TRASH_DEST].heaps_total
		  << " completed " << gstat.heaps_completed
		  << "=" << dstat[DATA_DEST].heaps_completed
		  << "+" << dstat[TEMP_DEST].heaps_completed
		  << "+" << dstat[TRASH_DEST].heaps_completed
		  << " discarded " << gstat.heaps_discarded
		  << "=" << dstat[DATA_DEST].heaps_discarded
		  << "+" << dstat[TEMP_DEST].heaps_discarded
		  << "+" << dstat[TRASH_DEST].heaps_discarded
		  << " open " << dstat[DATA_DEST].heaps_open << " " << dstat[TEMP_DEST].heaps_open
		  << " skipped " << gstat.heaps_skipped
		  << " overrun " << gstat.heaps_overrun
		  << " ignored " << gstat.heaps_ignored
		  << " assigned " << dest[DATA_DEST].count << " " << dest[TEMP_DEST].count << " " << dest[TRASH_DEST].count
		  << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed
		  << " payload " << gstat.bytes_expected << " " << gstat.bytes_received
		  << std::endl;
      }
  }

  void storage_single_buffer::show_state_log()
  {
    if (state == SEQUENTIAL_STATE)
      {
	std::cout << "-> sequential";
      }
    else
      {
	std::cout << "-> parallel";
      }
    std::cout << " total " << gstat.heaps_total
	      << " completed " << gstat.heaps_completed
	      << " discarded " << gstat.heaps_discarded
	      << " skipped " << gstat.heaps_skipped
	      << " overrun " << gstat.heaps_overrun
	      << " ignored " << gstat.heaps_ignored
	      << " assigned " << dest[DATA_DEST].count << " " << dest[TEMP_DEST].count << " " << dest[TRASH_DEST].count
	      << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed
              << " completed " << dstat[DATA_DEST].heaps_completed << " " << dstat[TEMP_DEST].heaps_completed
              << " open " << dstat[DATA_DEST].heaps_open << " " << dstat[TEMP_DEST].heaps_open
              << " level " << timestamp_level_data << " " << timestamp_level_temp
	      << " payload " << gstat.bytes_expected << " " << gstat.bytes_received
	      << std::endl;
    //hist.show();
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
