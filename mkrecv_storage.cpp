#include "mkrecv_storage.h"

namespace mkrecv
{

  storage::storage(std::shared_ptr<mkrecv::options> hopts) :
#ifndef USE_STD_MUTEX
    dest_sem(1),
#endif
    opts(hopts)
  {
    int i;
    
    heap_size = opts->heap_size;
    heap_count = 1;
    for (i = 0; i < opts->nindices; i++)
      {
	int count = opts->indices[i].values.size();
	if (count == 0) count = 1;
	heap_count *= count;
      }
    timestamp_step = opts->indices[0].step;
    nsci = opts->nsci;
    scis = opts->scis;
    memallocator = std::make_shared<spead2::mmap_allocator>(0, true);
  }

  storage::~storage()
  {
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::cout << "storage: ";
    et.show();
#endif
  }

  void storage::handle_init()
  {
  }
  
  int storage::handle_alloc_place(spead2::s_item_pointer_t &group_index, int dest_index)
  {
    (void)group_index;
    return dest_index;
  }

  int storage::alloc_place(spead2::s_item_pointer_t timestamp,     // timestamp of a heap
			   std::size_t heap_index,                 // heap number inside a heap group
			   std::size_t size,                       // heap size (only payload)
			   int dest_index,                         // requested destination (DATA_DEST or TRASH_DEST)
			   char *&heap_place,                      // returned memory pointer to this heap payload
			   spead2::s_item_pointer_t *&sci_place)   // returned memory pointer to the side-channel items for this heap
  {
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
    char                           *mem_base = NULL;
    //spead2::s_item_pointer_t        mem_offset;
    spead2::s_item_pointer_t       *sci_base = NULL;
    //spead2::s_item_pointer_t        sci_offset;
    spead2::s_item_pointer_t        group_index;
    spead2::s_item_pointer_t        offset;
    
    {
      // **** GUARDED BY SEMAPHORE ****
#ifdef USE_STD_MUTEX
      std::lock_guard<std::mutex> lock(dest_mutex);
#else
      dest_sem.get();
#endif
      if ((state == INIT_STATE) && (dest_index == DATA_DEST))
	{
	  if (heap_size == HEAP_SIZE_DEF) heap_size = size;
	  state = SEQUENTIAL_STATE;
	  std::cout << "sizes: heap size " << heap_size << " count " << heap_count << " first " << timestamp_first << " step " << timestamp_step << std::endl;
	  if (dest[DATA_DEST].active)
	    {
	      dest[DATA_DEST].set_heap_size(heap_size, heap_count, 0, nsci);
	      std::cout << "DATA_DEST:  capacity " << dest[DATA_DEST].capacity << " space " << dest[DATA_DEST].space << std::endl;
	    }
	  if (dest[TEMP_DEST].active)
	    {
	      dest[TEMP_DEST].set_heap_size(heap_size, heap_count, 0, nsci);
	      std::cout << "TEMP_DEST:  capacity " << dest[TEMP_DEST].capacity << " space " << dest[TEMP_DEST].space << std::endl;
	    }
	  if (dest[TRASH_DEST].active)
	    {
	      dest[TRASH_DEST].set_heap_size(heap_size, heap_count, 0, nsci);
	      std::cout << "TRASH_DEST: capacity " << dest[TRASH_DEST].capacity << " space " << dest[TRASH_DEST].space << std::endl;
	    }
	  timestamp_first = timestamp + 2*timestamp_step; // 2 is a safety margin to avoid incomplete heaps
	  timestamp_data_count = dest[TEMP_DEST].capacity + ((dest[DATA_DEST].capacity - dest[TEMP_DEST].capacity)*opts->level_data)/100;
	  timestamp_temp_count = dest[DATA_DEST].capacity + (dest[TEMP_DEST].capacity*opts->level_temp)/100;
	  timestamp_temp_level = timestamp_first + timestamp_temp_count*timestamp_step;
	  std::cout << "timestamp_data_count " << timestamp_data_count << " timestamp_temp_count " << timestamp_temp_count << " timestamp_temp_level " << timestamp_temp_level << std::endl;
	  handle_init();
	}
      if (stop)
	{
	  dest_index = TRASH_DEST;
	  has_stopped = true;
	}
      if (dest_index == TRASH_DEST)
	{
	  gstat.heaps_ignored++;
	}
      else
	{
	  group_index = (timestamp - timestamp_first)/timestamp_step;
	  dest_index = handle_alloc_place(group_index, dest_index);
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
      offset = group_index*heap_count + heap_index;
      heap_place = mem_base + heap_size*offset;
      sci_place  = sci_base + nsci*offset;
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
  
  void storage::handle_free_place(spead2::s_item_pointer_t timestamp, int dest_index)
  {
    (void)timestamp;
    (void)dest_index;
  }

  void storage::free_place(spead2::s_item_pointer_t timestamp,    // timestamp of a heap
			   int dest_index,                        // real destination of a heap (DATA_DEST, TEMP_DEST or TRASH_DEST)
			   std::size_t reclen)                    // recieved number of bytes
  {
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
    {
      // **** GUARDED BY SEMAPHORE ****
#ifdef USE_STD_MUTEX
      std::lock_guard<std::mutex> lock(dest_mutex);
#else
      dest_sem.get();
#endif
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
      handle_free_place(timestamp, dest_index);
      if ((gstat.heaps_total - log_counter) >= LOG_FREQ)
	{
	  log_counter += LOG_FREQ;
	  show_log();
	}
#ifndef USE_STD_MUTEX
      dest_sem.put();
#endif
    }
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    et.add_et(et_statistics::MARK_TIMING, std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count());
#endif
  }

  void storage::show_log()
  {
    std::cout << "heaps:";
    // total <n> = <data> + <temp> + <trash>
    std::cout << " total " << gstat.heaps_total << "=" << dstat[DATA_DEST].heaps_total;
    if (dest[TEMP_DEST].active) std::cout << "+" << dstat[TEMP_DEST].heaps_total;
    if (dest[TRASH_DEST].active) std::cout << "+" << dstat[TRASH_DEST].heaps_total;
    // completed <n> = <data> + <temp> + <trash>
    std::cout << " completed " << gstat.heaps_completed << "=" << dstat[DATA_DEST].heaps_completed;
    if (dest[TEMP_DEST].active) std::cout << "+" << dstat[TEMP_DEST].heaps_completed;
    if (dest[TRASH_DEST].active) std::cout << "+" << dstat[TRASH_DEST].heaps_completed;
    // discarded <n> = <data> + <temp> + <trash>
    std::cout << " discarded " << gstat.heaps_discarded << "=" << dstat[DATA_DEST].heaps_discarded;
    if (dest[TEMP_DEST].active) std::cout << "+" << dstat[TEMP_DEST].heaps_discarded;
    if (dest[TRASH_DEST].active) std::cout << "+" << dstat[TRASH_DEST].heaps_discarded;
    // open <n> = <data> + <temp> + <trash>
    std::cout << " open " << dstat[DATA_DEST].heaps_open;
    if (dest[TEMP_DEST].active) std::cout << " " << dstat[TEMP_DEST].heaps_open;
    if (dest[TRASH_DEST].active) std::cout << " " << dstat[TRASH_DEST].heaps_open;
    std::cout << " skipped " << gstat.heaps_skipped
	      << " overrun " << gstat.heaps_overrun
	      << " ignored " << gstat.heaps_ignored;
    // assigned <n> = <data> + <temp> + <trash>
    std::cout << " assigned " << dest[DATA_DEST].count;
    if (dest[TEMP_DEST].active) std::cout << " " << dest[TEMP_DEST].count;
    if (dest[TRASH_DEST].active) std::cout << " " << dest[TRASH_DEST].count;
    std::cout << " needed " << dest[DATA_DEST].needed;
    if (dest[TEMP_DEST].active) std::cout << " " << dest[TEMP_DEST].needed;
    if (dest[TRASH_DEST].active) std::cout << " " << dest[TRASH_DEST].needed;
    std::cout << " level " << timestamp_data_level << " " << timestamp_temp_level;
    std::cout << " payload " << gstat.bytes_expected << " " << gstat.bytes_received;
    std::cout << std::endl;
  }

}
