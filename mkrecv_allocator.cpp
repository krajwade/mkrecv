
#include "ascii_header.h"

#include "mkrecv_allocator.h"

namespace mkrecv
{

  allocator::allocator(key_t key, std::string mlname, std::shared_ptr<options> opts) :
    opts(opts),
    mlog(mlname),
    dada(key, mlog),
    hdr(NULL)
  {
    int i;

    memallocator = std::make_shared<spead2::mmap_allocator>(0, true);
    dada_mode = opts->dada_mode;
    if (dada_mode > 1)
      {
	hdr = &dada.header_stream().next();
	dest[DATA_DEST].set_buffer(&dada.data_stream().next(), dada.data_buffer_size());
      }
    else
      {
	dest[DATA_DEST].allocate_buffer(memallocator, MAX_DATA_SPACE);
      }
    dest[TEMP_DEST].allocate_buffer(memallocator, MAX_TEMPORARY_SPACE);
    dest[TRASH_DEST].allocate_buffer(memallocator, MAX_TEMPORARY_SPACE);
    std::cout << "dest[DATA_DEST].ptr.ptr()  = " << (std::size_t)(dest[DATA_DEST].ptr->ptr()) << std::endl;
    std::cout << "dest[TEMP_DEST].ptr.ptr()  = " << (std::size_t)(dest[TEMP_DEST].ptr->ptr()) << std::endl;
    std::cout << "dest[TRASH_DEST].ptr.ptr() = " << (std::size_t)(dest[TRASH_DEST].ptr->ptr()) << std::endl;
    heap_count = 1;
  }

  allocator::~allocator()
  {
  }

  void allocator::handle_dummy_heap(std::size_t size, void *hint)
  {
    spead2::recv::packet_header    *ph = (spead2::recv::packet_header*)hint;

    tstat.ntotal++;
    tstat.nignored++;
    dest[TRASH_DEST].count++;
    heap2dest[ph->heap_cnt] = TRASH_DEST;
    tstat.nexpected += size;
  }

  int allocator::handle_data_heap(std::size_t size, void *hint, std::size_t &heap_index)
  {
    return TRASH_DEST;
  }

  spead2::memory_allocator::pointer allocator::allocate(std::size_t size, void *hint)
  {
    spead2::recv::packet_header    *ph = (spead2::recv::packet_header*)hint;
    spead2::s_item_pointer_t        timestamp;
    int                             dest_index;
    std::size_t                     time_index;
    std::size_t                     heap_index;
    char*                           mem_base = NULL;
    std::size_t                     mem_offset;

    // Use semaphore to guard this method
    std::lock_guard<std::mutex> lock(dest_mutex);
    // Ignore heaps with cnt == 1 (no data heaps!)
    if (ph->heap_cnt == 1)
      {
	handle_dummy_heap(size, hint);
	return pointer((std::uint8_t*)(dest[dest_index].ptr->ptr()), deleter(shared_from_this(), (void *)std::uintptr_t(size)));
      }
    // Extract payload size and heap size as long as we are in INIT_STATE
    if (state == INIT_STATE)
      {
	payload_size = ph->payload_length;
	heap_size = size;
      }
    // Extract important items and calculate the destination and heap index
    dest_index = handle_data_heap(size, hint, heap_index);
    tstat.ntotal++;
    if (hasStopped)
      {
	tstat.nskipped++;
	dest_index = TRASH_DEST;
      }
    if (dest_index == TRASH_DEST)
      { // this heap goes into the trash can, do _not_ leave the INIT_STATE
	mem_base = dest[TRASH_DEST].ptr->ptr();
	dest[TRASH_DEST].count++;
	heap2dest[ph->heap_cnt] = TRASH_DEST; // store the relation between heap counter and destination
	tstat.nexpected += heap_size;
	return pointer((std::uint8_t*)(mem_base), deleter(shared_from_this(), (void *) std::uintptr_t(size)));
      }

    
    if (state == INIT_STATE)
      { // It is the first heap packet after startup (excluding heaps with cnt == 1)
	// Update the header and send it via a ringbuffer
	if (dada_mode >= 2)
	  {
	    memcpy(hdr->ptr(), opts->header, (DADA_DEFAULT_HEADER_SIZE < hdr->total_bytes()) ? DADA_DEFAULT_HEADER_SIZE : hdr->total_bytes());
	    hdr->used_bytes(hdr->total_bytes());
	    dada.header_stream().release();
	    hdr = NULL;
	  }
	// update internal bookkeeping numbers
	dest[DATA_DEST].set_heap_size(heap_size, heap_count);
	dest[TEMP_DEST].set_heap_size(heap_size, heap_count, 2);
	dest[TRASH_DEST].set_heap_size(heap_size, heap_count, 2);
	dest[TEMP_DEST].cts = 1;
	std::cout << "sizes: heap size " << heap_size << " count " << heap_count << " group first " << group_first << " step " << group_step << std::endl;
	std::cout << "DATA_DEST:  capacity " << dest[DATA_DEST].capacity << " space " << dest[DATA_DEST].space << std::endl;
	std::cout << "TEMP_DEST:  capacity " << dest[TEMP_DEST].capacity << " space " << dest[TEMP_DEST].space << std::endl;
	std::cout << "TRASH_DEST: capacity " << dest[TRASH_DEST].capacity << " space " << dest[TRASH_DEST].space << std::endl;
	std::cout << "heap " << ph->heap_cnt << std::endl;
	state = SEQUENTIAL_STATE;
      }
    if (state == SEQUENTIAL_STATE)
      {
	if (heap_index >= (dest[DATA_DEST].space + dest[TEMP_DEST].space))
	  {
	    tstat.noverrun++;
	    dest_index = TRASH_DEST;
	  }
	else if (heap_index >= dest[DATA_DEST].space)
	  {
	    dest_index = TEMP_DEST;
	    heap_index -= dest[DATA_DEST].space;
	  }
	else
	  {
	    dest_index = DATA_DEST;
	  }
      }
    else if (state == PARALLEL_STATE)
      {
	if (heap_index >= dest[DATA_DEST].space)
	  {
	    tstat.noverrun++;
	    dest_index = TRASH_DEST;
	  }
	else if (heap_index >= dest[TEMP_DEST].space)
	  {
	    dest_index = DATA_DEST;
	  }
	else
	  {
	    dest_index = TEMP_DEST;
	  }
      }
    if (dada_mode == 0)
      {
	dest_index = TRASH_DEST;
      }
    if (dest_index == TRASH_DEST)
      {
	heap_index = 0;
      }
    mem_offset = heap_size*heap_index;
    mem_base = dest[dest_index].ptr->ptr();
    dest[dest_index].count++;
    heap2dest[ph->heap_cnt] = dest_index; // store the relation between heap counter and destination
    tstat.nexpected += heap_size;
    /*
      std::cout << "heap " << ph->heap_cnt << " timestamp " << timestamp << " feng_id " << feng_id << " frequency " << frequency << " size " << size
      << " ->"
      << " dest " << d << " indices " << time_index << " " << feng_index << " " << freq_index
      << " offset " << mem_offset
      << " ntotal " << ntotal << " noverrun " << noverrun << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed
      << std::endl;
    */
    return pointer((std::uint8_t*)(mem_base + mem_offset), deleter(shared_from_this(), (void *) std::uintptr_t(size)));
  }

  void allocator::free(std::uint8_t *ptr, void *user)
  {
  }

  void allocator::handle_data_full()
  {
    std::lock_guard<std::mutex> lock(dest_mutex);
    if (dada_mode >= 3)
      {
	// release the current ringbuffer slot
	if (!hasStopped)
	  {
	    dest[DATA_DEST].ptr->used_bytes(dest[DATA_DEST].ptr->total_bytes());
	    dada.data_stream().release();
	  }
	// get a new ringbuffer slot
	if (stop)
	  {
	    hasStopped = true;
	    std::cout << "request to stop the transfer into the ringbuffer received." << std::endl;
	    dest[DATA_DEST].ptr     = &dada.data_stream().next();
	    dada.data_stream().release();
	    stop = false;
	  }
	if (!hasStopped)
	  {
	    dest[DATA_DEST].ptr     = &dada.data_stream().next();
	  }
      }
    dest[DATA_DEST].needed  = dest[DATA_DEST].space;
    group_first  += dest[DATA_DEST].capacity*group_step;
    dest[DATA_DEST].cts = heap_count;
    /*
      std::cout << "-> parallel total " << tstat.ntotal << " completed " << tstat.ncompleted << " discarded " << tstat.ndiscarded << " skipped " << tstat.nskipped << " overrun " << tstat.noverrun
      << " assigned " << dest[DATA_DEST].count << " " << dest[TEMP_DEST].count << " " << dest[TRASH_DEST].count
      << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed
      << " payload " << tstat.nexpected << " " << tstat.nreceived << std::endl;
    */
    // switch to parallel data/temp order
    state = PARALLEL_STATE;
  }

  void allocator::handle_temp_full()
  {
    if ((dada_mode >= 4) && !hasStopped)
      {
	memcpy(dest[DATA_DEST].ptr->ptr(), dest[TEMP_DEST].ptr->ptr(), dest[TEMP_DEST].space*heap_size);
      }
    //std::lock_guard<std::mutex> lock(dest_mutex);
    dest[TEMP_DEST].needed  = dest[TEMP_DEST].space;
    dest[DATA_DEST].needed -= dest[TEMP_DEST].space;
    dest[TEMP_DEST].cts = 1;
    /*
      std::cout << "-> sequential total " << tstat.ntotal << " completed " << tstat.ncompleted << " discarded " << tstat.ndiscarded << " skipped " << tstat.nskipped << " overrun " << tstat.noverrun << " ignored " << tstat.nignored
      << " assigned " << dest[DATA_DEST].count << " " << dest[TEMP_DEST].count << " " << dest[TRASH_DEST].count
      << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed
      << " payload " << tstat.nexpected << " " << tstat.nreceived << std::endl;
    */
    // switch to sequential data/temp order
    state = SEQUENTIAL_STATE;
  }

  /*
    void do_handle_data_full(allocator *rb)
    {
    rb->handle_data_full();
    }

    void do_handle_temp_full(allocator *rb)
    {
    rb->handle_temp_full();
    }
  */

  void allocator::mark_heap(spead2::s_item_pointer_t cnt, bool isok, spead2::s_item_pointer_t reclen)
  {
  }

  void allocator::mark_log()
  {
  }

  void allocator::mark(spead2::s_item_pointer_t cnt, bool isok, spead2::s_item_pointer_t reclen)
  {
    std::size_t  nd, nt, rt, ctsd, ctst;

    {
      std::lock_guard<std::mutex> lock(dest_mutex);
      int d = heap2dest[cnt];
      dest[d].needed--;
      dest[d].cts--;
      heap2dest.erase(cnt);
      tstat.nreceived += reclen;
      nd = dest[DATA_DEST].needed;
      nt = dest[TEMP_DEST].needed;
      rt = dest[TEMP_DEST].space - dest[TEMP_DEST].needed;
      ctsd = dest[DATA_DEST].cts;
      ctst = dest[TEMP_DEST].cts;
      if (!isok)
	{
	  tstat.ndiscarded++;
	}
      else
	{
	  tstat.ncompleted++;
	}
      mark_heap(cnt, isok, reclen);
      //std::cout << "mark " << cnt << " isok " << isok << " dest " << d << " needed " << nd << " " << nt << std::endl;
      if ((tstat.ntotal - log_counter) >= LOG_FREQ)
	{
	  int i;
	  log_counter += LOG_FREQ;
	  std::cout << "heaps:"
		    << " total " << tstat.ntotal
		    << " completed " << tstat.ncompleted
		    << " discarded " << tstat.ndiscarded
		    << " skipped " << tstat.nskipped
		    << " overrun " << tstat.noverrun
		    << " ignored " << tstat.nignored
		    << " assigned " << dest[DATA_DEST].count << " " << dest[TEMP_DEST].count << " " << dest[TRASH_DEST].count
		    << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed
		    << " error " << tstat.ntserror << " " << tstat.nbierror << " " << tstat.nfcerror
		    << " payload " << tstat.nexpected << " " << tstat.nreceived
		    << std::endl;
	  mark_log();
	}
      /*
      if (nd > 100000)
	{
	  std::cout << "mark " << cnt << " isok " << isok << " dest " << d << " needed " << nd << " " << nt << std::endl;
	  exit(0);
	}
      */
    }
    //if ((nd == 0) || (ctst == 0))
    if ((state == SEQUENTIAL_STATE) && (ctst == 0))
      {
	handle_data_full(); // std::thread dfull(do_handle_data_full, this); <- does not work, terminate
      }
    //else if ((nt == 0) || (ctsd == 0))
    else if ((state == PARALLEL_STATE) && (ctsd == 0))
      {
	handle_temp_full(); // std::thread tfull(do_handle_temp_full, this); <- does not work, terminate
      }
  }

  void allocator::request_stop()
  {
    stop = true;
  }

  bool allocator::is_stopped()
  {
    return hasStopped;
  }

}
