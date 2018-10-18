#include "mkrecv_storage_null.h"

namespace mkrecv
{

  storage_null::storage_null(std::shared_ptr<mkrecv::options> hopts) :
    storage(hopts)
  {
    std::size_t  data_size  = MAX_DATA_SPACE;
    std::size_t  temp_size  = MAX_TEMPORARY_SPACE;
    std::size_t  trash_size = MAX_TEMPORARY_SPACE;

    if (heap_size != HEAP_SIZE_DEF)
      {
	std::size_t group_size;
	group_size = heap_count*(heap_size + nsci*sizeof(spead2::s_item_pointer_t));
	data_size  = group_size; // 1 heap group
	temp_size  = group_size; // 1 heap group
	trash_size = group_size; // 1 heap group
      }
    dest[DATA_DEST].allocate_buffer(memallocator, data_size);
    dest[TEMP_DEST].allocate_buffer(memallocator, temp_size);
    dest[TRASH_DEST].allocate_buffer(memallocator, trash_size);
    std::cout << "dest[DATA_DEST].ptr.ptr()  = " << (std::size_t)(dest[DATA_DEST].ptr->ptr()) << std::endl;
    std::cout << "dest[TEMP_DEST].ptr.ptr()  = " << (std::size_t)(dest[TEMP_DEST].ptr->ptr()) << std::endl;
    std::cout << "dest[TRASH_DEST].ptr.ptr() = " << (std::size_t)(dest[TRASH_DEST].ptr->ptr()) << std::endl;
  }

  int storage_null::alloc_place(spead2::s_item_pointer_t timestamp,     // timestamp of a heap
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
	else if (group_index >= (spead2::s_item_pointer_t)(dest[DATA_DEST].capacity + dest[TEMP_DEST].capacity))
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
    else
      { // The requested destination is already TRASH_DEST -> This heap is should be ignored
	gstat.heaps_ignored++;
      }
    group_index = 0;
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
  
  void storage_null::free_place(int dest_index,           // destination of a heap
				std::size_t reclen)       // recieved number of bytes
  {
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
  }

  void storage_null::request_stop()
  {
    stop = true;
  }

  bool storage_null::is_stopped()
  {
    return has_stopped;
  }


}
