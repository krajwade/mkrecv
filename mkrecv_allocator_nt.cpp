#include <sched.h>

#ifdef ENABLE_TIMING_MEASUREMENTS
#include <chrono>
#endif

#include "ascii_header.h"

#include "mkrecv_allocator_nt.h"

namespace mkrecv
{

#ifdef ENABLE_CORE_MUTEX
  std::mutex allocator_nt::core_mutex[64];
#endif

  allocator_nt::allocator_nt(std::shared_ptr<options> opts, std::shared_ptr<mkrecv::storage> store) :
    opts(opts),
    store(store)
  {
    int i;

    // copy index_option into a local index_part
    nindices = opts->nindices;
    heap_count = 1;
    for (i = 0; i < MAX_INDEXPARTS; i++)
      {
	indices[i].set(opts->indices[i]);
	heap_count *= indices[i].count;
      }
    heap_size = opts->heap_size;
    nsci = opts->nsci;
    scis = opts->scis;
    //cts_data = dest[TEMP_DEST].space/4;
    //cts_temp = dest[TEMP_DEST].space/4;
    hist.set_step((std::int64_t)(indices[0].step));
  }

  allocator_nt::~allocator_nt()
  {
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::cout << "allocator_nt: ";
    et.show();
#endif
  }

  spead2::memory_allocator::pointer allocator_nt::allocate(std::size_t size, void *hint)
  {
    spead2::recv::packet_header    *ph = (spead2::recv::packet_header*)hint;
    spead2::recv::pointer_decoder   decoder(ph->heap_address_bits);
    spead2::s_item_pointer_t        item_value[MAX_INDEXPARTS];
    std::size_t                     item_index;
    int                             dest_index = storage::DATA_DEST;
    int                             odest_index = storage::DATA_DEST;
    std::size_t                     heap_index = 0;;
    char                           *heap_place = NULL;
    spead2::s_item_pointer_t       *sci_place = NULL;
    std::size_t                     i;
#ifdef ENABLE_CORE_MUTEX
    int                             coreid = sched_getcpu();
#endif

    // **** GUARDED BY SEMAPHORE ****
#ifdef ENABLE_CORE_MUTEX
    std::lock_guard<std::mutex> lock(core_mutex[coreid]);
#endif
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
    // Some heaps are ignored:
    //   If we have stopped
    //   If the heap is is 1
    //   If the heap size is wrong (needs a value for HEAP_SIZE of --heap-size)
    //   If there are not enough item pointers do calculate heap_index
    heaps_total++;
    bool ignore_heap = has_stopped;
    ignore_heap |= (ph->heap_cnt == 1);
    ignore_heap |= ((heap_size != 0) && (heap_size != size));
    ignore_heap |= ((std::size_t)(ph->n_items) < nindices);
    if (ignore_heap)
      {
        if (!has_stopped)
          {
	    std::cout << "heap ignored heap_cnt " << ph->heap_cnt << " size " << size << " n_items " << ph->n_items << '\n';
          }
	dest_index = store->alloc_place(0, 0, size, storage::TRASH_DEST, heap_place, sci_place);
	//heap2dest[ph->heap_cnt] = dest_index;
        heap_id[head] = ph->heap_cnt;
        heap_dest[head] = dest_index;
        heap_timestamp[head] = 0;
        inc(head, MAX_OPEN_HEAPS-1); // (head + 1)%MAX_OPEN_HEAPS;
	return pointer((std::uint8_t*)heap_place, deleter(shared_from_this(), (void *)std::uintptr_t(size)));
      }
    // Extract all item pointer values and transform them into a heap index (inside a heap group)
    for (i = 0; i < nindices; i++)
      {
	spead2::item_pointer_t pts = spead2::load_be<spead2::item_pointer_t>(ph->pointers + indices[i].item);
	// a mask is used to restrict the used bits of an item value (default -> use all bits)
	item_value[i] = (std::size_t)(decoder.get_immediate(pts) & indices[i].mask);
	if (i == 0) continue; // The timestamp is directly use by the storage to calculate the base adress of a heap group
	try {
	  item_index = indices[i].value2index.at(item_value[i]);
	  heap_index *= indices[i].count;
	  heap_index += item_index;
	}
	catch (const std::out_of_range& oor) {
	  // The item pointer value is unknown -> put this heap into the trash (ignore it)
	  indices[i].nskipped++;
	  dest_index = storage::TRASH_DEST;
	}
      }
    if (dest_index == storage::DATA_DEST)
      {
	hist.proc_ts((std::int64_t)(item_value[0]));
      }
    // If it is the first data heap and all , we have to setup some stuff
    if (!has_started && (dest_index == storage::DATA_DEST))
      {
	// Extract payload size and heap size as long as we are in INIT_STATE
	if (heap_size == 0) heap_size = size;
	std::cout << "first heap " << ph->heap_cnt << " size " << heap_size << '\n';
	has_started = true;
      }
    // Get the memory positions for the heap payload and the side-channel items
    odest_index = dest_index;
    dest_index = store->alloc_place(item_value[0], heap_index, size, dest_index, heap_place, sci_place);
    if ((dest_index == storage::TRASH_DEST) && (odest_index != storage::TRASH_DEST) && !opts->quiet) {
      std::cout << "HEAP " << ph->heap_cnt << " " << ph->heap_length << " " << ph->payload_offset << " " << ph->payload_length;
      for (i = 0; i < (size_t)(ph->n_items); i++) {
	spead2::item_pointer_t pts = spead2::load_be<spead2::item_pointer_t>(ph->pointers + i*sizeof(spead2::item_pointer_t));
	std::cout << " I[" << i << "] = " << decoder.get_immediate(pts) << " " << decoder.get_id(pts) << " " << decoder.get_immediate(pts);
      }
      std::cout << std::endl;
    }
    //std::cout << "heap " << ph->heap_cnt << " dest_index " << dest_index << " heap_index " << heap_index << " ts " << item_value[0] << "\n";
    //heap2dest[ph->heap_cnt]      = dest_index;    // store the relation between heap counter and destination
    //heap2timestamp[ph->heap_cnt] = item_value[0]; // store the relation between heap counter and timestamp
    //std::cout << "  heap map " << head << " " << heap_id[head] << " " << heap_dest[head] << " " << heap_timestamp[head] << '\n';
    heap_id[head]        = ph->heap_cnt;
    heap_dest[head]      = dest_index;
    heap_timestamp[head] = item_value[0];
    inc(head, MAX_OPEN_HEAPS-1); // (head + 1)%MAX_OPEN_HEAPS;
    for (i = 0; i < nsci; i++)
      { // Put all side-channel items directly into memory
	spead2::item_pointer_t pts = spead2::load_be<spead2::item_pointer_t>(ph->pointers + scis.at(i)*sizeof(spead2::item_pointer_t));
	sci_place[i] = decoder.get_immediate(pts);
      }
    /*
      std::cout << "heap " << ph->heap_cnt << " items " << indices[0].value << " " << indices[1].value << " " << indices[2].value << " size " << size
      << " ->"
      << " dest " << dest_index << " indices " << indices[0].index << " " << indices[1].index << " " << indices[2].index
      << " offset " << mem_offset
      << " ntotal " << tstat.ntotal << " noverrun " << tstat.noverrun << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed
      << '\n';
    */
#ifdef ENABLE_TIMING_MEASUREMENTS
    if (heaps_total == 2000000) et.reset();
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    et.add_et(et_statistics::ALLOC_TIMING, std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count());
#endif
    return pointer((std::uint8_t*)(heap_place), deleter(shared_from_this(), (void *) std::uintptr_t(size)));
  }

  void allocator_nt::mark(spead2::s_item_pointer_t cnt, bool isok, spead2::s_item_pointer_t reclen)
  {
    int                      dest_index;
    spead2::s_item_pointer_t timestamp;
#ifdef ENABLE_CORE_MUTEX
    int                      coreid = sched_getcpu();
#endif

    (void)isok;
    {
      // **** GUARDED BY SEMAPHORE ****
#ifdef ENABLE_CORE_MUTEX
      std::lock_guard<std::mutex> lock(core_mutex[coreid]);
#endif
#ifdef ENABLE_TIMING_MEASUREMENTS
      std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
      //std::cout << "mark " << cnt << " ok " << isok << " reclen " << reclen << "\n";
      std::size_t idx = head;
      std::size_t count = MAX_OPEN_HEAPS;
      dec(idx, MAX_OPEN_HEAPS-1); // heaps + (MAX_OPEN_HEAPS - 1))%MAX_OPEN_HEAPS;
      do
        {
          if (heap_id[idx] == cnt)
            {
              dest_index = heap_dest[idx];
              timestamp  = heap_timestamp[idx];
              break;
            }
         dec(idx, MAX_OPEN_HEAPS - 1);
         count--;
        } while (count != 0);
      if (count == 0)
        {
          std::cout << "ERROR: Cannot find heap with id " << cnt << " in internal map" << '\n';
          dest_index = storage::TRASH_DEST;
          timestamp = 0;
        }
      else
	{
	  std::size_t ohead = head;
	  dec(ohead,  MAX_OPEN_HEAPS-1); // The index of the latest entry
	  if (idx == ohead)
	    { // the marked head is the first in the internal map, we can remove this entry
	      // by setting the head ine step backwards and overwrite the latest entry next time a heap arrives
	      head = ohead;
	    }
	}
      //dest_index = heap2dest[cnt];
      //timestamp  = heap2timestamp[cnt];
      //heap2dest.erase(cnt);
      //heap2timestamp.erase(cnt);
#ifdef ENABLE_TIMING_MEASUREMENTS
      std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
      et.add_et(et_statistics::MARK_TIMING, std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count());
#endif
    }
    store->free_place(timestamp, dest_index, reclen);
  }

  void allocator_nt::request_stop()
  {
    stop = true;
  }

  bool allocator_nt::is_stopped()
  {
    return has_stopped;
  }

  void allocator_nt::free(std::uint8_t *ptr, void *user)
  {
    (void)ptr;
    (void)user;
  }


}
