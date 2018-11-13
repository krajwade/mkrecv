#include "mkrecv_storage_null.h"

namespace mkrecv
{

  storage_null::storage_null(std::shared_ptr<mkrecv::options> hopts, bool alloc_data) :
    storage(hopts)
  {
    std::size_t  data_size  = MAX_DATA_SPACE;

    if (heap_size != HEAP_SIZE_DEF)
      {
	std::size_t group_size;
	group_size = heap_count*(heap_size + nsci*sizeof(spead2::s_item_pointer_t));
	data_size  = opts->ngroups_data*group_size; // N heap groups (N = option NGROUPS_DATA)
      }
    if (alloc_data)
      {
	dest[DATA_DEST].allocate_buffer(memallocator, data_size);
      }
    if (alloc_data)
      {
	std::cout << "dest[DATA_DEST].ptr.ptr()  = " << (std::size_t)(dest[DATA_DEST].ptr->ptr()) << std::endl;
      }
  }

  storage_null::~storage_null()
  {
  }

  void storage_null::do_init(spead2::s_item_pointer_t timestamp,     // timestamp of a heap
			     std::size_t size                        // heap size (only payload)
			     )
  {
    (void)timestamp;
    state = SEQUENTIAL_STATE;
    if (heap_size == HEAP_SIZE_DEF) heap_size = size;
    dest[DATA_DEST].set_heap_size(heap_size, heap_count, 0, nsci);
    std::cout << "sizes: heap size " << heap_size << " count " << heap_count << std::endl;
    std::cout << "DATA_DEST:  capacity " << dest[DATA_DEST].capacity << " space " << dest[DATA_DEST].space << std::endl;
  }
  
  int storage_null::alloc_place(spead2::s_item_pointer_t timestamp,     // timestamp of a heap
				std::size_t heap_index,                 // heap number inside a heap group
				std::size_t size,                       // heap size (only payload)
				int dest_index,                         // requested destination (DATA_DEST or TRASH_DEST)
				char *&heap_place,                      // returned memory pointer to this heap payload
				spead2::s_item_pointer_t *&sci_place)   // returned memory pointer to the side-channel items for this heap
  {
    (void)timestamp;
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
    char                           *mem_base = NULL;
    spead2::s_item_pointer_t        mem_offset;
    spead2::s_item_pointer_t       *sci_base = NULL;
    spead2::s_item_pointer_t        sci_offset;

    {
    // **** GUARDED BY SEMAPHORE ****
#ifdef USE_STD_MUTEX
    std::lock_guard<std::mutex> lock(dest_mutex);
#else
    dest_sem.get();
#endif
    if ((state == INIT_STATE) && (dest_index == DATA_DEST))
      {
	do_init(timestamp, size);
      }
    if (stop)
      {
	dest_index = TRASH_DEST;
	has_stopped = true;
      }
    if (dest_index != DATA_DEST)
      { // The requested destination is already TRASH_DEST -> This heap is should be ignored
	gstat.heaps_ignored++;
      }
    dest_index = DATA_DEST;
    dest[dest_index].count++;
    gstat.heaps_total++;
    gstat.heaps_open++;
    gstat.bytes_expected += size;
    dstat[dest_index].heaps_total++;
    dstat[dest_index].heaps_open++;
    dstat[dest_index].bytes_expected += size;
    mem_base = dest[dest_index].ptr->ptr();
    sci_base = dest[dest_index].sci;
    mem_offset = heap_size*index_next;
    sci_offset = nsci*index_next;
    heap_place = mem_base + mem_offset;
    sci_place  = sci_base + sci_offset;
    index_next = (index_next + 1)%dest[DATA_DEST].space;
#ifndef USE_STD_MUTEX
    dest_sem.put();
#endif
    }
#ifdef ENABLE_TIMING_MEASUREMENTS
    if (gstat.heaps_total == 10*dest[DATA_DEST].size) et.reset();
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    et.add_et(et_statistics::ALLOC_TIMING, std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count());
#endif
    return dest_index;
  }
  
  void storage_null::free_place(spead2::s_item_pointer_t timestamp,    // timestamp of a heap
				int dest_index,                        // destination of a heap
				std::size_t reclen)                    // recieved number of bytes
  {
    (void)timestamp;
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
    //std::size_t  ctsd, ctst;

    {
    // **** GUARDED BY SEMAPHORE ****
#ifdef USE_STD_MUTEX
    std::lock_guard<std::mutex> lock(dest_mutex);
#else
    dest_sem.get();
#endif
    dest_index = DATA_DEST;
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
    show_mark_log();
#ifndef USE_STD_MUTEX
    dest_sem.put();
#endif
    }
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    et.add_et(et_statistics::MARK_TIMING, std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count());
#endif
  }

  void storage_null::show_mark_log()
  {
    if ((gstat.heaps_total - log_counter) >= LOG_FREQ)
      {
	log_counter += LOG_FREQ;
	std::cout << "heaps:"
		  << " total "
		  << gstat.heaps_total
		  << "=" << dstat[DATA_DEST].heaps_total
		  << " completed " << gstat.heaps_completed
		  << "=" << dstat[DATA_DEST].heaps_completed
		  << " discarded " << gstat.heaps_discarded
		  << "=" << dstat[DATA_DEST].heaps_discarded
		  << " open " << dstat[DATA_DEST].heaps_open 
		  << " skipped " << gstat.heaps_skipped
		  << " overrun " << gstat.heaps_overrun
		  << " ignored " << gstat.heaps_ignored
		  << " assigned " << dest[DATA_DEST].count
		  << " needed " << dest[DATA_DEST].needed
		  << " payload " << gstat.bytes_expected << " " << gstat.bytes_received
		  << std::endl;
      }
  }

  void storage_null::request_stop()
  {
    stop = true;
  }

  bool storage_null::is_stopped()
  {
    return has_stopped;
  }

  void storage_null::close()
  {
  }


}
