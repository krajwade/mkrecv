
#include "ascii_header.h"

#include "mkrecv_allocator.h"

namespace mkrecv
{

  index_part::index_part()
  {
  }

  void index_part::set(const index_options &opt)
  {
    int  i;

    item  = opt.item*sizeof(spead2::item_pointer_t);
    mask  = opt.mask;
    step  = opt.step;
    count = opt.values.size();
    if (count == 0) count = 1;
    for (i = 0; i < opt.values.size(); i++)
      {
	values.push_back(opt.values[i]);
	value2index[opt.values[i]] = i;
      }
  }

  allocator::allocator(key_t key, std::string mlname, std::shared_ptr<options> opts) :
    opts(opts),
    mlog(mlname),
    dada(key, mlog),
    hdr(NULL)
  {
    int i, j;
    std::size_t  data_size  = MAX_DATA_SPACE;
    std::size_t  temp_size  = MAX_TEMPORARY_SPACE;
    std::size_t  trash_size = MAX_TEMPORARY_SPACE;

    memallocator = std::make_shared<spead2::mmap_allocator>(0, true);
    // copy index_option into a local index_part
    nindices = opts->nindices;
    heap_count = 1;
    for (i = 0; i < MAX_INDEXPARTS; i++)
      {
	indices[i].set(opts->indices[i]);
	heap_count *= indices[i].count;
      }
    cts_data = opts->sources.size();
    cts_temp = opts->sources.size();
    heap_size = opts->heap_size;
    nsci = opts->nsci;
    scis = opts->scis;
    // allocate memory
    if (heap_size != HEAP_SIZE_DEF)
      {
	std::size_t group_size;
	group_size = heap_count*(heap_size + nsci*sizeof(spead2::s_item_pointer_t));
	data_size  = 4*opts->ngroups_temp*group_size; // 4*N heap groups (N = option NGROUPS_TEMP)
	temp_size  =   opts->ngroups_temp*group_size; //   N heap groups (N = option NGROUPS_TEMP)
	trash_size =                      group_size; //   1 heap group
      }
    dada_mode = opts->dada_mode;
    if (dada_mode >= STATIC_DADA_MODE)
      {
	hdr = &dada.header_stream().next();
	dest[DATA_DEST].set_buffer(&dada.data_stream().next(), dada.data_buffer_size());
      }
    else
      {
	dest[DATA_DEST].allocate_buffer(memallocator, data_size);
      }
    dest[TEMP_DEST].allocate_buffer(memallocator, temp_size);
    dest[TRASH_DEST].allocate_buffer(memallocator, trash_size);
    std::cout << "dest[DATA_DEST].ptr.ptr()  = " << (std::size_t)(dest[DATA_DEST].ptr->ptr()) << std::endl;
    std::cout << "dest[TEMP_DEST].ptr.ptr()  = " << (std::size_t)(dest[TEMP_DEST].ptr->ptr()) << std::endl;
    std::cout << "dest[TRASH_DEST].ptr.ptr() = " << (std::size_t)(dest[TRASH_DEST].ptr->ptr()) << std::endl;
  }

  allocator::~allocator()
  {
  }

  spead2::memory_allocator::pointer allocator::allocate(std::size_t size, void *hint)
  {
    spead2::recv::packet_header    *ph = (spead2::recv::packet_header*)hint;
    spead2::recv::pointer_decoder   decoder(ph->heap_address_bits);
    int                             i;
    spead2::s_item_pointer_t        item_value[MAX_INDEXPARTS];
    std::size_t                     item_index[MAX_INDEXPARTS];
    int                             dest_index = DATA_DEST;
    std::size_t                     heap_index;
    char*                           mem_base = NULL;
    std::size_t                     mem_offset;
    spead2::s_item_pointer_t       *sci_base = NULL;

    // **** GUARDED BY SEMAPHORE ****
    std::lock_guard<std::mutex> lock(dest_mutex);
    tstat.ntotal++;
    // Ignore heaps which have a id (cnt) equal to 1, the wrong heap size or if we have stopped
    bool ignore_heap = hasStopped;
    ignore_heap |= (ph->heap_cnt == 1);
    ignore_heap |= ((heap_size != HEAP_SIZE_DEF) && (heap_size != size));
    ignore_heap |= (ph->n_items < nindices);
    if (ignore_heap)
      {
	std::cout << "heap ignored heap_cnt " << ph->heap_cnt << " size " << size << " n_items " << ph->n_items << std::endl;
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
	// a mask is used to restrict the used bits of an item value (default -> use all bits)
	item_value[i] = decoder.get_immediate(pts) & indices[i].mask;
      }
    // calculate the index from the timestamp (first index component)
    if (state == INIT_STATE)
      { // Set the first running index value (first item pointer used for indexing)
	indices[0].first = item_value[0] + 2*indices[0].step; // 2 is a safety margin to avoid incomplete heaps
      }
    item_index[0] = (item_value[0] - indices[0].first)/indices[0].step;
    // All other index components are used in the same way
    for (i = 1; i < nindices; i++)
      {
	try {
	  item_index[i] = indices[i].value2index.at(item_value[i]);
	}
	catch (const std::out_of_range& oor) {
	  item_index[i] = 0;
	  indices[i].nskipped++;
	  dest_index = TRASH_DEST;
	}
      }
    if (dest_index == TRASH_DEST) tstat.nskipped++;
    // If it is the first data heap, we have to setup some stuff
    if (state == INIT_STATE)
      {
	// Extract payload size and heap size as long as we are in INIT_STATE
	payload_size = ph->payload_length;
	if (heap_size == HEAP_SIZE_DEF) heap_size = size;
	dest[DATA_DEST].set_heap_size(heap_size, heap_count, 0, nsci);
	dest[TEMP_DEST].set_heap_size(heap_size, heap_count, 0, nsci);
	dest[TRASH_DEST].set_heap_size(heap_size, heap_count, 0, nsci);
	dest[DATA_DEST].cts = cts_data;
	dest[TEMP_DEST].cts = cts_temp;
	state = SEQUENTIAL_STATE;
	
	if (dada_mode >= STATIC_DADA_MODE)
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
    // Calculate all indices and check their value.
    // The first index component is something special because it is unique
    // and it is used to select between DATA_DEST and TEMP_DEST.
    if ((item_value[0] > indices[0].first) && (item_index[0] >= 4*dest[DATA_DEST].capacity))
      {
	indices[0].nerror++;
	dest_index = TRASH_DEST;
      }
    else if (item_value[0] < indices[0].first)
      {
	tstat.nskipped++;
	indices[0].nskipped++;
	dest_index = TRASH_DEST;
      }
    else if (state == SEQUENTIAL_STATE)
      {
	if (item_index[0] >= (dest[DATA_DEST].capacity + dest[TEMP_DEST].capacity))
	  {
	    tstat.noverrun++;
	    dest_index = TRASH_DEST;
	      if (hasStarted)
	      {
	      std::cout << "SEQ overrun: " << ph->heap_cnt << " " << item_value[0] << " " << item_index[0] << " " << indices[0].first << " " << indices[0].step << std::endl;
	      //if (tstat.noverrun == 1000) exit(1);
	      }
	  }
	else if (item_index[0] >= dest[DATA_DEST].capacity)
	  {
	    dest_index = TEMP_DEST;
	    item_index[0] -= dest[DATA_DEST].capacity;
	  }
      }
    else if (state == PARALLEL_STATE)
      {
	if (item_index[0] >= dest[DATA_DEST].capacity)
	  {
	    tstat.noverrun++;
	    dest_index = TRASH_DEST;
	      std::cout << "PAR overrun: " << ph->heap_cnt << " " << item_value[0] << " " << item_index[0] << " " << indices[0].first << " " << indices[0].step << std::endl;
	      if (tstat.noverrun == 100) exit(1);
	  }
	else if (item_index[0] < dest[TEMP_DEST].capacity)
	  {
	    dest_index = TEMP_DEST;
	  }
      }
    // calculate the heap index, for example Timestamp -> Engine/Board/Antenna -> Frequency -> Time -> Polarization -> Complex number
    if (dada_mode == NO_DADA_MODE) dest_index = TRASH_DEST;
    if (dest_index == TRASH_DEST) item_index[0] = 0;
    heap_index = item_index[0];
    for (i = 1; i < nindices; i++)
      {
	heap_index *= indices[i].count;
	heap_index += item_index[i];
      }
    if (dest_index == DATA_DEST) hasStarted = true;
    mem_offset = heap_size*heap_index;
    mem_base = dest[dest_index].ptr->ptr();
    sci_base = dest[dest_index].sci;
    for (i = 0; i < nsci; i++)
      {
	spead2::item_pointer_t pts = spead2::load_be<spead2::item_pointer_t>(ph->pointers + scis.at(i)*sizeof(spead2::item_pointer_t));
	sci_base[heap_index*nsci + i] = decoder.get_immediate(pts);
      }
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

  void allocator::show_mark_log()
  {
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
	std::cout << " error " << tstat.nerror;
	for (i = 0; i < nindices; i++)
	  {
	    std::cout << " " << indices[i].nerror;
	  }
	std::cout << " payload " << tstat.nexpected << " " << tstat.nreceived
		  << " cts " << dest[DATA_DEST].cts << " " << dest[TEMP_DEST].cts
		  << std::endl;
      }
  }

  void allocator::show_state_log()
  {
    if (state == SEQUENTIAL_STATE)
      {
	std::cout << "-> sequential";
      }
    else
      {
	std::cout << "-> parallel";
      }
    std::cout << " total " << tstat.ntotal
	      << " completed " << tstat.ncompleted
	      << " discarded " << tstat.ndiscarded
	      << " skipped " << tstat.nskipped
	      << " overrun " << tstat.noverrun
	      << " ignored " << tstat.nignored
	      << " assigned " << dest[DATA_DEST].count << " " << dest[TEMP_DEST].count << " " << dest[TRASH_DEST].count
	      << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed
	      << " payload " << tstat.nexpected << " " << tstat.nreceived
	      << " cts " << dest[DATA_DEST].cts << " " << dest[TEMP_DEST].cts << std::endl;
  }

  void allocator::mark(spead2::s_item_pointer_t cnt, bool isok, spead2::s_item_pointer_t reclen)
  {
    std::size_t  nd, nt, ctsd, ctst, i, j;

    // **** GUARDED BY SEMAPHORE ****
    std::lock_guard<std::mutex> lock(dest_mutex);
    int d = heap2dest[cnt];
    dest[d].needed--;
    dest[d].cts--;
    heap2dest.erase(cnt);
    tstat.nreceived += reclen;
    nd = dest[DATA_DEST].needed;
    nt = dest[TEMP_DEST].needed;
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
    show_mark_log();
    //std::cout << "mark " << cnt << " isok " << isok << " dest " << d << " needed " << nd << " " << nt << " cts " << ctsd << " " << ctst << std::endl;
    if ((state == SEQUENTIAL_STATE) && (ctst == 0))
      {
	if (!hasStopped && (dada_mode >= DYNAMIC_DADA_MODE))
	  { // copy the optional side-channel items at the correct position
	    // sci_base = buffer + size - (scape *nsci)
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
	if (stop)
	  {
	    hasStopped = true;
	    std::cout << "request to stop the transfer into the ringbuffer received." << std::endl;
	    if (dada_mode >= DYNAMIC_DADA_MODE)
	      { // release the previously allocated slot without any data -> used as end signal
		dada.data_stream().release();
	      }
	    stop = false;
	  }
	dest[DATA_DEST].needed  = dest[DATA_DEST].space;
	indices[0].first  += dest[DATA_DEST].capacity*indices[0].step;
	dest[DATA_DEST].cts = cts_data;
	// switch to parallel data/temp order
	state = PARALLEL_STATE;
	show_state_log();
      }
    else if ((state == PARALLEL_STATE) && (ctsd == 0))
      {
	if (!hasStopped && (dada_mode >= FULL_DADA_MODE))
	  { // copy the heaps in temporary space into data space
	    memcpy(dest[DATA_DEST].ptr->ptr(), dest[TEMP_DEST].ptr->ptr(), dest[TEMP_DEST].space*heap_size);
	    if (nsci != 0)
	      { // copy the side-channel items in temporary space into data space and clear the source
		memcpy(dest[DATA_DEST].sci, dest[TEMP_DEST].sci, dest[TEMP_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
		memset(dest[TEMP_DEST].sci, 0, dest[TEMP_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
	      }
	  }
	dest[TEMP_DEST].needed  = dest[TEMP_DEST].space;
	dest[DATA_DEST].needed -= dest[TEMP_DEST].space;
	dest[TEMP_DEST].cts = cts_temp;
	// switch to sequential data/temp order
	state = SEQUENTIAL_STATE;
	show_state_log();
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
