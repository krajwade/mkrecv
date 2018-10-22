#include "mkrecv_storage_full_dada.h"

namespace mkrecv
{

  storage_full_dada::storage_full_dada(std::shared_ptr<mkrecv::options> hopts, key_t key, std::string mlname) :
    storage(hopts),
    mlog(mlname),
    dada(key, mlog),
    hdr(NULL)
  {
    std::size_t  temp_size  = MAX_TEMPORARY_SPACE;
    std::size_t  trash_size = MAX_TEMPORARY_SPACE;

    if (heap_size != HEAP_SIZE_DEF)
      {
	std::size_t group_size;
	group_size = heap_count*(heap_size + nsci*sizeof(spead2::s_item_pointer_t));
	temp_size  =   opts->ngroups_temp*group_size; //   N heap groups (N = option NGROUPS_TEMP)
	trash_size =                      group_size; //   1 heap group
      }
    hdr = &dada.header_stream().next();
    dest[DATA_DEST].set_buffer(&dada.data_stream().next(), dada.data_buffer_size());
    dest[TEMP_DEST].allocate_buffer(memallocator, temp_size);
    dest[TRASH_DEST].allocate_buffer(memallocator, trash_size);
    cts_data = opts->nheaps_switch;
    if (cts_data == NHEAPS_SWITCH_DEF)
      {
	cts_data = dest[TEMP_DEST].space/4;
	if (cts_data < heap_count) cts_data = heap_count;
      }
    cts_temp = cts_data;
    std::cout << "dest[DATA_DEST].ptr.ptr()  = " << (std::size_t)(dest[DATA_DEST].ptr->ptr()) << std::endl;
    std::cout << "dest[TEMP_DEST].ptr.ptr()  = " << (std::size_t)(dest[TEMP_DEST].ptr->ptr()) << std::endl;
    std::cout << "dest[TRASH_DEST].ptr.ptr() = " << (std::size_t)(dest[TRASH_DEST].ptr->ptr()) << std::endl;
  }

  void storage_full_dada::proc_header()
  {
    opts->set_start_time(timestamp_first);
    memcpy(hdr->ptr(),
	   opts->header,
	   (DADA_DEFAULT_HEADER_SIZE < hdr->total_bytes()) ? DADA_DEFAULT_HEADER_SIZE : hdr->total_bytes());
    hdr->used_bytes(hdr->total_bytes());
    dada.header_stream().release();
    hdr = NULL;
  }
  
  int storage_full_dada::alloc_place(spead2::s_item_pointer_t timestamp,     // timestamp of a heap
				     std::size_t heap_index,                 // heap number inside a heap group
				     std::size_t size,                       // heap size (only payload)
				     int dest_index,                         // requested destination (DATA_DEST or TRASH_DEST)
				     char *&heap_place,                      // returned memory pointer to this heap payload
				     spead2::s_item_pointer_t *&sci_place)   // returned memory pointer to the side-channel items for this heap
  {
    char                           *mem_base = NULL;
    spead2::s_item_pointer_t        mem_offset;
    spead2::s_item_pointer_t       *sci_base = NULL;
    spead2::s_item_pointer_t        sci_offset;
    spead2::s_item_pointer_t        group_index;

    // **** GUARDED BY SEMAPHORE ****
    std::lock_guard<std::mutex> lock(dest_mutex);
    if ((state == INIT_STATE) && (dest_index == DATA_DEST))
      {
	if (heap_size == HEAP_SIZE_DEF) heap_size = size;
	timestamp_first = timestamp + 2*timestamp_step; // 2 is a safety margin to avoid incomplete heaps
	dest[DATA_DEST].set_heap_size(heap_size, heap_count, 0, nsci);
	dest[TEMP_DEST].set_heap_size(heap_size, heap_count, 0, nsci);
	dest[TRASH_DEST].set_heap_size(heap_size, heap_count, 0, nsci);
	dest[DATA_DEST].cts = cts_data;
	dest[TEMP_DEST].cts = cts_temp;
	state = SEQUENTIAL_STATE;
	std::cout << "sizes: heap size " << heap_size << " count " << heap_count << " first " << timestamp_first << " step " << timestamp_step << std::endl;
	std::cout << "DATA_DEST:  capacity " << dest[DATA_DEST].capacity << " space " << dest[DATA_DEST].space << std::endl;
	std::cout << "TEMP_DEST:  capacity " << dest[TEMP_DEST].capacity << " space " << dest[TEMP_DEST].space << std::endl;
	std::cout << "TRASH_DEST: capacity " << dest[TRASH_DEST].capacity << " space " << dest[TRASH_DEST].space << std::endl;
	std::cout << "cts " << dest[DATA_DEST].cts << " " << dest[TEMP_DEST].cts << " " << dest[TRASH_DEST].cts << std::endl;
	header_thread = std::thread([this] ()
				    {
				      this->proc_header();
				    });
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
	    //std::cout << "TS too old: " << timestamp << " " << timestamp_first << " " << timestamp_step << " -> " << group_index << std::endl;
	    dest_index = TRASH_DEST;
	  }
	else if (state == SEQUENTIAL_STATE)
	  {
	    if (group_index >= (spead2::s_item_pointer_t)(dest[DATA_DEST].capacity + dest[TEMP_DEST].capacity))
	      {
		gstat.heaps_overrun++;
		//std::cout << "SEQ overrun: " << timestamp << " " << timestamp_first << " " << timestamp_step << " -> " << group_index << std::endl;
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
		//if (tstat.noverrun == 100) exit(1);
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
    return dest_index;
  }

  void storage_full_dada::proc_switch_slot()
  {
    spead2::s_item_pointer_t  *sci_base = (spead2::s_item_pointer_t*)(dest[DATA_DEST].ptr->ptr()
								      + dest[DATA_DEST].size
								      - dest[DATA_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
    memcpy(sci_base, dest[DATA_DEST].sci, dest[DATA_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
    memset(dest[DATA_DEST].sci, 0, dest[DATA_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
    // Release the current slot and get a new one
    dest[DATA_DEST].ptr->used_bytes(dest[DATA_DEST].ptr->total_bytes());
    dada.data_stream().release();
    dest[DATA_DEST].ptr = &dada.data_stream().next();
  }

  void storage_full_dada::proc_copy_temp()
  {
    memcpy(dest[DATA_DEST].ptr->ptr(), dest[TEMP_DEST].ptr->ptr(), dest[TEMP_DEST].space*heap_size);
    if (nsci != 0)
      { // copy the side-channel items in temporary space into data space and clear the source
	memcpy(dest[DATA_DEST].sci, dest[TEMP_DEST].sci, dest[TEMP_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
	memset(dest[TEMP_DEST].sci, 0, dest[TEMP_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
      }
  }
  
  void storage_full_dada::free_place(int dest_index,           // destination of a heap
				     std::size_t reclen)       // recieved number of bytes
  {
    std::size_t  ctsd, ctst;

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
    dest[dest_index].cts--;
    ctsd = dest[DATA_DEST].cts;
    ctst = dest[TEMP_DEST].cts;
    show_mark_log();
    //std::cout << "mark " << cnt << " isok " << isok << " dest " << d << " needed " << nd << " " << nt << " cts " << ctsd << " " << ctst << std::endl;
    if ((state == SEQUENTIAL_STATE) && (ctst == 0))
      {
	// switch to parallel data/temp order
        std::cout << "still needing " << dest[DATA_DEST].needed << " heaps." << std::endl;
	state = PARALLEL_STATE;
	if (!has_stopped)
	  { // copy the optional side-channel items at the correct position
	    // sci_base = buffer + size - (scape *nsci)
	    if (copy_thread.joinable()) copy_thread.join(); // We have to wait that the temporary heaps are copied into the slot before we switch to the next slot!
	    switch_thread = std::thread([this] ()
					{
					  this->proc_switch_slot();
					});
	  }
	if (stop && !has_stopped)
	  {
	    has_stopped = true;
	    std::cout << "request to stop the transfer into the ringbuffer received." << std::endl;
	    // release the previously allocated slot without any data -> used as end signal
	    dada.data_stream().release();
	    //stop = false;
            //hist.show();
	  }
	dest[DATA_DEST].needed  = dest[DATA_DEST].space;
	timestamp_first  += dest[DATA_DEST].capacity*timestamp_step;
	dest[DATA_DEST].cts = cts_data;
	show_state_log();
      }
    else if ((state == PARALLEL_STATE) && (ctsd == 0))
      {
	// switch to sequential data/temp order
	state = SEQUENTIAL_STATE;
	if (!has_stopped)
	  { // copy the heaps in temporary space into data space
	    if (switch_thread.joinable()) switch_thread.join(); // We have to wait until a new slot is available before we copy the temporary heaps into a slot!
	    copy_thread = std::thread([this] ()
				      {
					this->proc_copy_temp();
				      });
	  }
	dest[TEMP_DEST].needed  = dest[TEMP_DEST].space;
	dest[DATA_DEST].needed -= dest[TEMP_DEST].space;
	dest[TEMP_DEST].cts = cts_temp;
	show_state_log();
      }
  }

  void storage_full_dada::show_mark_log()
  {
    if ((gstat.heaps_total - log_counter) >= LOG_FREQ)
      {
	log_counter += LOG_FREQ;
	std::cout << "heaps:"
		  << " total "
		  << gstat.heaps_total
		  << "=" << gstat.heaps_total
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
		  << " cts " << dest[DATA_DEST].cts << " " << dest[TEMP_DEST].cts
		  << std::endl;
      }
  }

  void storage_full_dada::show_state_log()
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
	      << " payload " << gstat.bytes_expected << " " << gstat.bytes_received
	      << " cts " << dest[DATA_DEST].cts << " " << dest[TEMP_DEST].cts << std::endl;
    //hist.show();
  }

  void storage_full_dada::request_stop()
  {
    stop = true;
  }

  bool storage_full_dada::is_stopped()
  {
    return has_stopped;
  }

  void storage_full_dada::close()
  {
    // Avoid messages when exiting the program
    if (header_thread.joinable()) header_thread.join();
    if (switch_thread.joinable()) switch_thread.join();
    if (copy_thread.joinable()) copy_thread.join();
  }
  

}
