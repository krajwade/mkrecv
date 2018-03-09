
#include "ascii_header.h"

#include "mkrecv_allocator.h"

namespace mkrecv
{

  index_part::index_part()
  {
    int i;

    for (i = 0; i < MAX_VALUE; i++)
      {
	hcount[i] = 0;
      }
  }

  void index_part::set(const index_options &opt)
  {
    item  = opt.item*sizeof(spead2::item_pointer_t);
    first = opt.first;
    step  = opt.step;
    count = opt.count;
  }

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
    // copy index_option into a local index_part
    nindices = opts->nindices;
    heap_count = 1;
    for (i = 0; i < MAX_INDEXPARTS; i++)
      {
	indices[i].set(opts->indices[i]);
	heap_count *= indices[i].count;
      }
  }

  allocator::~allocator()
  {
  }

  int allocator::handle_data_heap(std::size_t size, void *hint, std::size_t &heap_index)
  {
    return TRASH_DEST;
  }

  /*
    direct access to the item pointers is used instead of the folowing piece of code:
    int its = 0, ibi = 0, ifi = 0;
    for (int i = 0; i < ph->n_items; i++)
      {
	spead2::item_pointer_t pointer = spead2::load_be<spead2::item_pointer_t>(ph->pointers + i * sizeof(spead2::item_pointer_t));
	bool special;
	if (decoder.is_immediate(pointer))
	  {
	    switch (decoder.get_id(pointer))
	      {
	      case TIMESTAMP_ID:
		its = i;
		otimestamp = timestamp = decoder.get_immediate(pointer);
		break;
	      case FENG_ID_ID:
		ibi = i;
		ofeng_id = feng_id = decoder.get_immediate(pointer);
		break;
	      case FREQUENCY_ID:
		ifi = i;
		ofrequency = frequency = decoder.get_immediate(pointer);
		break;
	      default:
		break;
	      }
	  }
      }
   */

  spead2::memory_allocator::pointer allocator::allocate(std::size_t size, void *hint)
  {
    spead2::recv::packet_header    *ph = (spead2::recv::packet_header*)hint;
    spead2::recv::pointer_decoder   decoder(ph->heap_address_bits);
    int                             i;
    int                             dest_index;
    std::size_t                     heap_index;
    char*                           mem_base = NULL;
    std::size_t                     mem_offset;

    // Use semaphore to guard this method
    std::lock_guard<std::mutex> lock(dest_mutex);
    // Ignore heaps which have a id (cnt) equal to 1, these are no data heaps
    if (ph->heap_cnt == 1)
      {
	tstat.ntotal++;
	tstat.nignored++;
	dest[TRASH_DEST].count++;
	heap2dest[ph->heap_cnt] = TRASH_DEST;
	tstat.nexpected += size;
	return pointer((std::uint8_t*)(dest[TRASH_DEST].ptr->ptr()), deleter(shared_from_this(), (void *)std::uintptr_t(size)));
      }
    // Extract all item pointer values used for index calculation
    for (i = 0; i < nindices; i++)
      {
	spead2::item_pointer_t pts = spead2::load_be<spead2::item_pointer_t>(ph->pointers + indices[i].item);
	indices[i].value = decoder.get_immediate(pts);
      }
    // If it is the first data heap, we have to setup some stuff
    if (state == INIT_STATE)
      {
	// Extract payload size and heap size as long as we are in INIT_STATE
	payload_size = ph->payload_length;
	heap_size = size;
	dest[DATA_DEST].set_heap_size(heap_size, heap_count);
	dest[TEMP_DEST].set_heap_size(heap_size, heap_count, 2);
	dest[TRASH_DEST].set_heap_size(heap_size, heap_count, 2);
	dest[TEMP_DEST].cts = 1;
	state = SEQUENTIAL_STATE;
	// Set the first running index value (first item pointer used for indexing)
	indices[0].first = indices[0].value + 2*indices[0].step; // 2 is a safety margin to avoid incomplete heaps
	if (dada_mode >= 2)
	  {
	    opts->set_start_time(indices[0].first);
            memcpy(hdr->ptr(), opts->header, (DADA_DEFAULT_HEADER_SIZE < hdr->total_bytes()) ? DADA_DEFAULT_HEADER_SIZE : hdr->total_bytes());
	    hdr->used_bytes(hdr->total_bytes());
	    dada.header_stream().release();
	    hdr = NULL;
          }
	std::cout << "sizes: heap size " << heap_size << " count " << heap_count << " first " << indices[0].first << " step " << indices[0].step << std::endl;
	std::cout << "DATA_DEST:  capacity " << dest[DATA_DEST].capacity << " space " << dest[DATA_DEST].space << std::endl;
	std::cout << "TEMP_DEST:  capacity " << dest[TEMP_DEST].capacity << " space " << dest[TEMP_DEST].space << std::endl;
	std::cout << "TRASH_DEST: capacity " << dest[TRASH_DEST].capacity << " space " << dest[TRASH_DEST].space << std::endl;
	std::cout << "heap " << ph->heap_cnt << " cts " << dest[DATA_DEST].cts << " " << dest[TEMP_DEST].cts << " " << dest[TRASH_DEST].cts << std::endl;
      }
    // Assume that this heap will go into a ringbuffer
    dest_index = DATA_DEST;
    tstat.ntotal++;
    if (hasStopped)
      { // If the process has stopped, any heap will go into the trash
	tstat.nskipped++;
	dest_index = TRASH_DEST;
      }
    else
      {
	// Calculate all indices and check their value.
	// The first index component is something special because it is unique
	// and it is used to select between DATA_DEST and TEMP_DEST.
	indices[0].index = (indices[0].value - indices[0].first)/indices[0].step;
	if ((indices[0].value > indices[0].first) && (indices[0].index >= 4*dest[DATA_DEST].capacity))
	  {
	    indices[0].nerror++;
	    dest_index = TRASH_DEST;
	  }
	else if (indices[0].value < indices[0].first)
	  {
	    tstat.nskipped++;
	    indices[0].nskipped++;
	    dest_index = TRASH_DEST;
	  }
	else if (state == SEQUENTIAL_STATE)
	  {
	    if (indices[0].index >= (dest[DATA_DEST].capacity + dest[TEMP_DEST].capacity))
	      {
		tstat.noverrun++;
		dest_index = TRASH_DEST;
	      }
	    else if (indices[0].index >= dest[DATA_DEST].capacity)
	      {
		dest_index = TEMP_DEST;
		indices[0].index -= dest[DATA_DEST].capacity;
	      }
	  }
	else if (state == PARALLEL_STATE)
	  {
	    if (indices[0].index >= dest[DATA_DEST].capacity)
	      {
		tstat.noverrun++;
		dest_index = TRASH_DEST;
	      }
	    else if (indices[0].index < dest[TEMP_DEST].capacity)
	      {
		dest_index = TEMP_DEST;
	      }
	  }
	// All other index components are used in the same way
	for (i = 1; i < nindices; i++)
	  {
	    indices[i].index = (indices[i].value - indices[i].first)/indices[i].step;
	    if ((indices[i].value < indices[i].first) || (indices[i].index >= indices[i].count))
	      {
		tstat.nskipped++;
		indices[i].nskipped++;
		dest_index = TRASH_DEST;
	      }
	    if ((indices[i].value >= 0) && (indices[i].value < MAX_VALUE))
	      {
		indices[i].hcount[indices[i].value]++;
	      }
	    else
	      {
		indices[i].ocount++;
	      }
	  }
      }
    // calculate the heap index, for example Timestamp -> Engine/Board/Antenna -> Frequency -> Time -> Polarization -> Complex number
    heap_index = indices[0].index;
    for (i = 1; i < nindices; i++)
      {
	heap_index *= indices[i].count;
	heap_index += indices[i].index;
      }
    // It is the first heap packet after startup, update the header and send it via a ringbuffer
    /*
    if (state == INIT_STATE)
      { 
	if (dada_mode >= 2)
	  {
	    memcpy(hdr->ptr(), opts->header, (DADA_DEFAULT_HEADER_SIZE < hdr->total_bytes()) ? DADA_DEFAULT_HEADER_SIZE : hdr->total_bytes());
	    hdr->used_bytes(hdr->total_bytes());
	    dada.header_stream().release();
	    hdr = NULL;
	  }
	// update internal bookkeeping numbers
	dest[TEMP_DEST].cts = 1;
	std::cout << "sizes: heap size " << heap_size << " count " << heap_count << " group first " << indices[0].first << " step " << indices[0].step << std::endl;
	std::cout << "DATA_DEST:  capacity " << dest[DATA_DEST].capacity << " space " << dest[DATA_DEST].space << std::endl;
	std::cout << "TEMP_DEST:  capacity " << dest[TEMP_DEST].capacity << " space " << dest[TEMP_DEST].space << std::endl;
	std::cout << "TRASH_DEST: capacity " << dest[TRASH_DEST].capacity << " space " << dest[TRASH_DEST].space << std::endl;
	std::cout << "heap " << ph->heap_cnt << std::endl;
	state = SEQUENTIAL_STATE;
      }
    */
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
      std::cout << "heap " << ph->heap_cnt << " items " << indices[0].value << " " << indices[1].value << " " << indices[2].value << " size " << size
      << " ->"
      << " dest " << dest_index << " indices " << indices[0].index << " " << indices[1].index << " " << indices[2].index
      << " offset " << mem_offset
      << " ntotal " << tstat.ntotal << " noverrun " << tstat.noverrun << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed
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
    indices[0].first  += dest[DATA_DEST].capacity*indices[0].step;
    dest[DATA_DEST].cts = heap_count;
    // /*
      std::cout << "-> parallel total " << tstat.ntotal << " completed " << tstat.ncompleted << " discarded " << tstat.ndiscarded << " skipped " << tstat.nskipped << " overrun " << tstat.noverrun
      << " assigned " << dest[DATA_DEST].count << " " << dest[TEMP_DEST].count << " " << dest[TRASH_DEST].count
      << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed
      << " payload " << tstat.nexpected << " " << tstat.nreceived << std::endl;
    // */
    // switch to parallel data/temp order
    state = PARALLEL_STATE;
  }

  void allocator::handle_temp_full()
  {
    if ((dada_mode >= 4) && !hasStopped)
      {
	memcpy(dest[DATA_DEST].ptr->ptr(), dest[TEMP_DEST].ptr->ptr(), dest[TEMP_DEST].space*heap_size);
      }
    std::lock_guard<std::mutex> lock(dest_mutex);
    dest[TEMP_DEST].needed  = dest[TEMP_DEST].space;
    dest[DATA_DEST].needed -= dest[TEMP_DEST].space;
    dest[TEMP_DEST].cts = 1;
    // /*
      std::cout << "-> sequential total " << tstat.ntotal << " completed " << tstat.ncompleted << " discarded " << tstat.ndiscarded << " skipped " << tstat.nskipped << " overrun " << tstat.noverrun << " ignored " << tstat.nignored
      << " assigned " << dest[DATA_DEST].count << " " << dest[TEMP_DEST].count << " " << dest[TRASH_DEST].count
      << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed
      << " payload " << tstat.nexpected << " " << tstat.nreceived << std::endl;
    // */
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

  void allocator::mark(spead2::s_item_pointer_t cnt, bool isok, spead2::s_item_pointer_t reclen)
  {
    std::size_t  nd, nt, rt, ctsd, ctst, i, j;

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
      //std::cout << "mark " << cnt << " isok " << isok << " dest " << d << " needed " << nd << " " << nt << " cts " << ctsd << " " << ctst << std::endl;
      if ((tstat.ntotal - log_counter) >= LOG_FREQ)
	{
	  int i;
	  log_counter += LOG_FREQ;
	  std::cout << "heaps:"
		    << " total " << tstat.ntotal
		    << " completed " << tstat.ncompleted
		    << " discarded " << tstat.ndiscarded;
	  std::cout << " skipped " << tstat.nskipped;
	  for (i = 0; i < nindices; i++)
	    {
	      std::cout << " " << indices[i].nskipped;
	    }
	  std::cout << " overrun " << tstat.noverrun
		    << " ignored " << tstat.nignored
		    << " assigned " << dest[DATA_DEST].count << " " << dest[TEMP_DEST].count << " " << dest[TRASH_DEST].count
		    << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed;
	  std::cout << " error";
	  for (i = 0; i < nindices; i++)
	    {
	      std::cout << " " << indices[i].nerror;
	    }
	  std::cout << " payload " << tstat.nexpected << " " << tstat.nreceived
		    << " cts " << ctsd << " " << ctst
		    << std::endl;
	  if (log_counter <= LOG_FREQ)
	    {
	      for (i = 1; i < nindices; i++)
		{
		  for (j = 0; j < MAX_VALUE; j++)
		    {
		      if (indices[i].hcount[j] == 0) continue;
		      std::cout << "item " << i << ", value " << j << " count " << indices[i].hcount[j] << std::endl;
		    }
		}
	    }
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
