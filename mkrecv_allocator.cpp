#include <chrono>

#include "ascii_header.h"

#include "mkrecv_allocator.h"

namespace mkrecv
{

  index_part::index_part()
  {
  }

  index_part::~index_part()
  {
    if (map != NULL) {
      delete map;
      map = NULL;
    }
  }

  void index_part::set(const index_options &opt, std::size_t index_size)
  {
    size_t  i;
    spead2::s_item_pointer_t  val;
    size_t                    valsize  = 0;
    spead2::s_item_pointer_t  valmask  = 0;

    item  = opt.item*sizeof(spead2::item_pointer_t);
    mask  = opt.mask;
    step  = opt.step;
    count = opt.values.size();
    std::cout << "count " << count << " list items\n";
    if (count == 0) count = 1;
    for (i = 0; i < opt.values.size(); i++) {
      spead2::s_item_pointer_t k = opt.values[i]; // key   := element in the option list (--idx<i>-list)
      std::size_t              v = i*index_size;  // value := heap index [B] calculated from list position and index_size
      if ((i == 0) || (k < valmin)) valmin = k;
      valbits |= k;
      values.push_back(k);
      value2index[k] = v;
      std::cout << "  hmap " << k << " -> " << v << '\n';
    }
    if (valbits != 0) {
      val = valbits;
      while ((val & 1) == 0) {
	valshift++;
	val = val >> 1;
      }
      valmax = val;
      while (val != 0) {
	valsize++;
	val = val >> 1;
      }
      valmask = (1 << valsize) - 1;
    }
    std::cout << "  valbits " << valbits << " size " << valsize << " shift " << valshift << " valmask " << valmask << " valmin " << valmin << " valmax " << valmax << '\n';
    if (valmax < 4096) {
      // we use a direct approach for mapping pointer item values into indices
      map = new std::size_t[valmax + 1];
      for (val = 0; val <= valmax; val++) {
	map[val] = (std::size_t)-1;
      }
      for (i = 0; i < opt.values.size(); i++) {
	spead2::s_item_pointer_t k   = opt.values[i]; // key   := element in the option list (--idx<i>-list)
	std::size_t              v   = i*index_size;  // value := heap index [B] calculated from list position and index_size
	spead2::s_item_pointer_t idx = (k >> valshift);  // index in direct map (k shifted by valshift in order to remove the 0-bits)
	map[idx] = v;
	std::cout << "  dmap " << k << " -> " << idx << " -> " << v << '\n';
      }
    }
  }

  std::size_t index_part::v2i(spead2::s_item_pointer_t v, bool uhm)
  {
    if (uhm || (map == NULL)) {
      try {
	return value2index.at(v);
      }
      catch (const std::out_of_range& oor) {
	return (std::size_t)-1;
      }
    } else if (v & ~valbits) {
      return -1;
    } else if (v < valmin) {
      return -1;
    } else {
      spead2::s_item_pointer_t hv = v >> valshift;
      if (hv > valmax) return -1;
      return map[hv];
    }
  }


  ts_histo::ts_histo()
  {
    int i;

    has_last_ts = false;
    ts_step = 1;
    last_ts = 0;
    most_negative_index = 0;
    count_negative_indices = 0;
    for (i = 0; i < 2*TS_HISTO_SLOTS+1; i++)
      {
        counts[i] = 0;
      }
    most_positive_index = 0;
    count_positive_indices = 0;
  }

  void ts_histo::set_step(std::int64_t step)
  {
    ts_step = step;
  }

  void ts_histo::proc_ts(std::int64_t ts)
  {
    if (!has_last_ts)
      {
        last_ts = ts;
        has_last_ts = true;
      }
    std::int64_t idx = (ts - last_ts)/ts_step;
    if (idx < -TS_HISTO_SLOTS)
      {
        if (idx < most_negative_index) most_negative_index = idx;
        count_negative_indices++;
      }
    else if (idx > TS_HISTO_SLOTS)
      {
        if (idx > most_positive_index) most_positive_index = idx;
        count_positive_indices++;
      }
    else
      {
        counts[idx + TS_HISTO_SLOTS]++;
      }
    if (ts > last_ts)
      {
        last_ts = ts;
      }
  }

  void ts_histo::show()
  {
    int i;

    std::cout << "ts histo: " << most_negative_index << " .. " << most_positive_index;
    std::cout << " neg " << count_negative_indices;
    for (i = 0; i < 2*TS_HISTO_SLOTS+1; i++)
      {
        std::cout << " " << (i - TS_HISTO_SLOTS) << ":" << counts[i];
      }
    std::cout << " pos " << count_positive_indices;
    std::cout << '\n';
  }

#ifdef ENABLE_TIMING_MEASUREMENTS
  et_statistics::et_statistics()
  {
    int i;
    
    for (i = 0; i < 2; i++)
      {
	min_et[i] = 1.0e30;
	max_et[i] = 0.0;
	sum_et[i] = 0.0;
	count_et[i] = 0.0;
      }
    for (i = 0; i < MAX_SLOTS; i++)
      {
	histo[0][i] = 0;
	histo[1][i] = 0;
      }
  }
  /*
   * 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000
   * 100000, 200000, 500000, 1000000, 2000000, 5000000
   * 10000000, 20000000, 50000000, 100000000, 200000000, 500000000
   */
  void et_statistics::add_et(int fi, double nanoseconds)
  {
    if (nanoseconds < min_et[fi]) min_et[fi] = nanoseconds;
    if (nanoseconds > max_et[fi]) max_et[fi] = nanoseconds;
    sum_et[fi]   += nanoseconds;
    count_et[fi] += 1.0;
    if (nanoseconds <= 10.0)
      {
	histo[fi][0]++;
      }
    else if (nanoseconds <= 100.0)
      {
	if (nanoseconds <= 20.0)
	  {
	    histo[fi][1]++;
	  }
	else if (nanoseconds <= 50.0)
	  {
	    histo[fi][2]++;
	  }
	else
	  {
	    histo[fi][3]++;
	  }
      }
    else if (nanoseconds <= 1000.0)
      {
	if (nanoseconds <= 200.0)
	  {
	    histo[fi][4]++;
	  }
	else if (nanoseconds <= 500.0)
	  {
	    histo[fi][5]++;
	  }
	else
	  {
	    histo[fi][6]++;
	  }
      }
    else if (nanoseconds <= 10000.0)
      {
	if (nanoseconds <= 2000.0)
	  {
	    histo[fi][7]++;
	  }
	else if (nanoseconds <= 5000.0)
	  {
	    histo[fi][8]++;
	  }
	else
	  {
	    histo[fi][9]++;
	  }
      }
    else if (nanoseconds <= 100000.0)
      {
	if (nanoseconds <= 20000.0)
	  {
	    histo[fi][10]++;
	  }
	else if (nanoseconds <= 50000.0)
	  {
	    histo[fi][11]++;
	  }
	else
	  {
	    histo[fi][12]++;
	  }
      }
    else if (nanoseconds <= 1000000.0)
      {
	if (nanoseconds <= 200000.0)
	  {
	    histo[fi][13]++;
	  }
	else if (nanoseconds <= 500000.0)
	  {
	    histo[fi][14]++;
	  }
	else
	  {
	    histo[fi][15]++;
	  }
      }
    else if (nanoseconds <= 10000000.0)
      {
	if (nanoseconds <= 2000000.0)
	  {
	    histo[fi][16]++;
	  }
	else if (nanoseconds <= 5000000.0)
	  {
	    histo[fi][17]++;
	  }
	else
	  {
	    histo[fi][18]++;
	  }
      }
    else if (nanoseconds <= 100000000.0)
      {
	if (nanoseconds <= 20000000.0)
	  {
	    histo[fi][19]++;
	  }
	else if (nanoseconds <= 50000000.0)
	  {
	    histo[fi][20]++;
	  }
	else
	  {
	    histo[fi][21]++;
	  }
      }
    else if (nanoseconds <= 1000000000.0)
      {
	if (nanoseconds <= 200000000.0)
	  {
	    histo[fi][22]++;
	  }
	else if (nanoseconds <= 500000000.0)
	  {
	    histo[fi][23]++;
	  }
	else
	  {
	    histo[fi][24]++;
	  }
      }
    else
      {
	histo[fi][LARGER_SLOT]++;
      }
  }
  
  void et_statistics::show()
  {
    /*
    std::cout << "et: alloc " << sum_et[ALLOC_TIMING]/count_et[ALLOC_TIMING] << " [" << min_et[ALLOC_TIMING] << " .. " << max_et[ALLOC_TIMING] << "]";
    std::cout << " <= 10 ns = " << histo[ALLOC_TIMING][0];
    std::cout << " <= 20 ns = " << histo[ALLOC_TIMING][1];
    std::cout << " <= 50 ns = " << histo[ALLOC_TIMING][2];
    std::cout << " <= 100 ns = " << histo[ALLOC_TIMING][3];
    std::cout << " <= 200 ns = " << histo[ALLOC_TIMING][4];
    std::cout << " <= 500 ns = " << histo[ALLOC_TIMING][5];
    std::cout << " <= 1 us = " << histo[ALLOC_TIMING][6];
    std::cout << " <= 2 us = " << histo[ALLOC_TIMING][7];
    std::cout << " <= 5 us = " << histo[ALLOC_TIMING][8];
    std::cout << " <= 10 us = " << histo[ALLOC_TIMING][9];
    std::cout << " <= 20 us = " << histo[ALLOC_TIMING][10];
    std::cout << " <= 50 us = " << histo[ALLOC_TIMING][11];
    std::cout << " <= 100 us = " << histo[ALLOC_TIMING][12];
    std::cout << " <= 200 us = " << histo[ALLOC_TIMING][13];
    std::cout << " <= 500 us = " << histo[ALLOC_TIMING][14];
    std::cout << " <= 1 ms = " << histo[ALLOC_TIMING][15];
    std::cout << " <= 2 ms = " << histo[ALLOC_TIMING][16];
    std::cout << " <= 5 ms = " << histo[ALLOC_TIMING][17];
    std::cout << " <= 10 ms = " << histo[ALLOC_TIMING][18];
    std::cout << " <= 20 ms = " << histo[ALLOC_TIMING][19];
    std::cout << " <= 50 ms = " << histo[ALLOC_TIMING][20];
    std::cout << " <= 100 ms = " << histo[ALLOC_TIMING][21];
    std::cout << " <= 200 ms = " << histo[ALLOC_TIMING][22];
    std::cout << " <= 500 ms = " << histo[ALLOC_TIMING][23];
    std::cout << " > 500 ms = " << histo[ALLOC_TIMING][LARGER_SLOT];
    std::cout << '\n';
    
    std::cout << "et: mark " << sum_et[MARK_TIMING]/count_et[MARK_TIMING] << " [" << min_et[MARK_TIMING] << " .. " << max_et[MARK_TIMING] << "]";
    std::cout << " <= 10 ns = " << histo[MARK_TIMING][0];
    std::cout << " <= 20 ns = " << histo[MARK_TIMING][1];
    std::cout << " <= 50 ns = " << histo[MARK_TIMING][2];
    std::cout << " <= 100 ns = " << histo[MARK_TIMING][3];
    std::cout << " <= 200 ns = " << histo[MARK_TIMING][4];
    std::cout << " <= 500 ns = " << histo[MARK_TIMING][5];
    std::cout << " <= 1 us = " << histo[MARK_TIMING][6];
    std::cout << " <= 2 us = " << histo[MARK_TIMING][7];
    std::cout << " <= 5 us = " << histo[MARK_TIMING][8];
    std::cout << " <= 10 us = " << histo[MARK_TIMING][9];
    std::cout << " <= 20 us = " << histo[MARK_TIMING][10];
    std::cout << " <= 50 us = " << histo[MARK_TIMING][11];
    std::cout << " <= 100 us = " << histo[MARK_TIMING][12];
    std::cout << " <= 200 us = " << histo[MARK_TIMING][13];
    std::cout << " <= 500 us = " << histo[MARK_TIMING][14];
    std::cout << " <= 1 ms = " << histo[MARK_TIMING][15];
    std::cout << " <= 2 ms = " << histo[MARK_TIMING][16];
    std::cout << " <= 5 ms = " << histo[MARK_TIMING][17];
    std::cout << " <= 10 ms = " << histo[MARK_TIMING][18];
    std::cout << " <= 20 ms = " << histo[MARK_TIMING][19];
    std::cout << " <= 50 ms = " << histo[MARK_TIMING][20];
    std::cout << " <= 100 ms = " << histo[MARK_TIMING][21];
    std::cout << " <= 200 ms = " << histo[MARK_TIMING][22];
    std::cout << " <= 500 ms = " << histo[MARK_TIMING][23];
    std::cout << " > 500 ms = " << histo[MARK_TIMING][LARGER_SLOT];
    std::cout << '\n';
    */
    std::cout << "et:"
              << " alloc " << sum_et[ALLOC_TIMING]/count_et[ALLOC_TIMING] << " [" << min_et[ALLOC_TIMING] << " .. " << max_et[ALLOC_TIMING] << "]"
              << " mark " << sum_et[MARK_TIMING]/count_et[MARK_TIMING] << " [" << min_et[MARK_TIMING] << " .. " << max_et[MARK_TIMING] << "]"
              << '\n';
    std::cout << "10 " << histo[ALLOC_TIMING][0] << " " << histo[MARK_TIMING][0] << '\n';
    std::cout << "20 " << histo[ALLOC_TIMING][1] << " " << histo[MARK_TIMING][1] << '\n';
    std::cout << "50 " << histo[ALLOC_TIMING][2] << " " << histo[MARK_TIMING][2] << '\n';
    std::cout << "100 " << histo[ALLOC_TIMING][3] << " " << histo[MARK_TIMING][3] << '\n';
    std::cout << "200 " << histo[ALLOC_TIMING][4] << " " << histo[MARK_TIMING][4] << '\n';
    std::cout << "500 " << histo[ALLOC_TIMING][5] << " " << histo[MARK_TIMING][5] << '\n';
    std::cout << "1000 " << histo[ALLOC_TIMING][6] << " " << histo[MARK_TIMING][6] << '\n';
    std::cout << "2000 " << histo[ALLOC_TIMING][7] << " " << histo[MARK_TIMING][7] << '\n';
    std::cout << "5000 " << histo[ALLOC_TIMING][8] << " " << histo[MARK_TIMING][8] << '\n';
    std::cout << "10000 " << histo[ALLOC_TIMING][9] << " " << histo[MARK_TIMING][9] << '\n';
    std::cout << "20000 " << histo[ALLOC_TIMING][10] << " " << histo[MARK_TIMING][10] << '\n';
    std::cout << "50000 " << histo[ALLOC_TIMING][11] << " " << histo[MARK_TIMING][11] << '\n';
    std::cout << "100000 " << histo[ALLOC_TIMING][12] << " " << histo[MARK_TIMING][12] << '\n';
    std::cout << "200000 " << histo[ALLOC_TIMING][13] << " " << histo[MARK_TIMING][13] << '\n';
    std::cout << "500000 " << histo[ALLOC_TIMING][14] << " " << histo[MARK_TIMING][14] << '\n';
    std::cout << "1000000 " << histo[ALLOC_TIMING][15] << " " << histo[MARK_TIMING][15] << '\n';
    std::cout << "2000000 " << histo[ALLOC_TIMING][16] << " " << histo[MARK_TIMING][16] << '\n';
    std::cout << "5000000 " << histo[ALLOC_TIMING][17] << " " << histo[MARK_TIMING][17] << '\n';
    std::cout << "10000000 " << histo[ALLOC_TIMING][18] << " " << histo[MARK_TIMING][18] << '\n';
    std::cout << "20000000 " << histo[ALLOC_TIMING][19] << " " << histo[MARK_TIMING][19] << '\n';
    std::cout << "50000000 " << histo[ALLOC_TIMING][20] << " " << histo[MARK_TIMING][20] << '\n';
    std::cout << "100000000 " << histo[ALLOC_TIMING][21] << " " << histo[MARK_TIMING][21] << '\n';
    std::cout << "200000000 " << histo[ALLOC_TIMING][22] << " " << histo[MARK_TIMING][22] << '\n';
    std::cout << "500000000 " << histo[ALLOC_TIMING][23] << " " << histo[MARK_TIMING][23] << '\n';
  }

  void et_statistics::reset()
  {
    int i;
    
    for (i = 0; i < 2; i++)
      {
	min_et[i] = 1.0e30;
	max_et[i] = 0.0;
	sum_et[i] = 0.0;
	count_et[i] = 0.0;
      }
    for (i = 0; i < MAX_SLOTS; i++)
      {
        histo[0][i] = 0;
        histo[1][i] = 0;
      }
  }
#endif

  allocator::allocator(key_t key, std::string mlname, std::shared_ptr<options> opts) :
    opts(opts),
    mlog(mlname),
    dada(key, mlog),
    hdr(NULL)
  {
    int i;
    std::size_t  index_size = 1;
    std::size_t  data_size  = MAX_DATA_SPACE;
    std::size_t  temp_size  = MAX_TEMPORARY_SPACE;
    std::size_t  trash_size = MAX_TEMPORARY_SPACE;

    memallocator = std::make_shared<spead2::mmap_allocator>(0, true);
    // copy index_option into a local index_part
    nindices = opts->nindices;
    for (i = nindices - 1; i >= 0; i--) {
      indices[i].set(opts->indices[i], index_size);
      index_size *= indices[i].count;
    }
    heap_count = index_size;
    heap_size = opts->heap_size;
    nsci = opts->nsci;
    scis = opts->scis;
    // allocate memory
    if (heap_size != 0)
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
    cts_data = opts->nheaps_switch;
    if (cts_data == 0)
      {
	cts_data = dest[TEMP_DEST].space/4;
	if (cts_data < heap_count) cts_data = heap_count;
      }
    cts_temp = cts_data;
    hist.set_step((std::int64_t)(indices[0].step));
    std::cout << "dest[DATA_DEST].ptr.ptr()  = " << (std::size_t)(dest[DATA_DEST].ptr->ptr()) << '\n';
    std::cout << "dest[TEMP_DEST].ptr.ptr()  = " << (std::size_t)(dest[TEMP_DEST].ptr->ptr()) << '\n';
    std::cout << "dest[TRASH_DEST].ptr.ptr() = " << (std::size_t)(dest[TRASH_DEST].ptr->ptr()) << '\n';
  }

  allocator::~allocator()
  {
#ifdef ENABLE_TIMING_MEASUREMENTS
    et.show();
#endif
  }

  spead2::memory_allocator::pointer allocator::allocate(std::size_t size, void *hint)
  {
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
    spead2::recv::packet_header    *ph = (spead2::recv::packet_header*)hint;
    spead2::recv::pointer_decoder   decoder(ph->heap_address_bits);
    std::size_t                     i;
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
    ignore_heap |= ((heap_size != 0) && (heap_size != size));
    ignore_heap |= ((std::size_t)(ph->n_items) < nindices);
    if (ignore_heap)
      {
        if (!hasStopped)
          {
	    std::cout << "heap ignored heap_cnt " << ph->heap_cnt << " size " << size << " n_items " << ph->n_items << '\n';
          }
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
	item_value[i] = (std::size_t)(decoder.get_immediate(pts) & indices[i].mask);
      }
    // calculate the index from the timestamp (first index component)
    if (state == INIT_STATE)
      { // Set the first running index value (first item pointer used for indexing)
	indices[0].first = item_value[0] + 2*indices[0].step; // 2 is a safety margin to avoid incomplete heaps
      }
    item_index[0] = (item_value[0] - indices[0].first)/indices[0].step;
    hist.proc_ts((std::int64_t)(item_value[0]));
    // All other index components are used in the same way
    for (i = 1; i < nindices; i++)
      {
	item_index[i] = indices[i].v2i(item_value[i]);
	if (item_index[i] == (std::size_t)-1) {
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
	if (heap_size == 0) heap_size = size;
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
	std::cout << "sizes: heap size " << heap_size << " count " << heap_count << " first " << indices[0].first << " step " << indices[0].step << '\n';
	std::cout << "DATA_DEST:  capacity " << dest[DATA_DEST].capacity << " space " << dest[DATA_DEST].space << '\n';
	std::cout << "TEMP_DEST:  capacity " << dest[TEMP_DEST].capacity << " space " << dest[TEMP_DEST].space << '\n';
	std::cout << "TRASH_DEST: capacity " << dest[TRASH_DEST].capacity << " space " << dest[TRASH_DEST].space << '\n';
	std::cout << "heap " << ph->heap_cnt << " cts " << dest[DATA_DEST].cts << " " << dest[TEMP_DEST].cts << " " << dest[TRASH_DEST].cts << '\n';
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
	//std::cout << "TS too old: " << ph->heap_cnt << " " << item_value[0] << " " << item_index[0] << " " << indices[0].first << " " << indices[0].step << " diff " << (indices[0].first - item_value[0])/indices[0].step << '\n';
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
	      std::cout << "SEQ overrun: " << ph->heap_cnt << " " << item_value[0] << " " << item_index[0] << " " << indices[0].first << " " << indices[0].step << '\n';
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
	      std::cout << "PAR overrun: " << ph->heap_cnt << " " << item_value[0] << " " << item_index[0] << " " << indices[0].first << " " << indices[0].step << '\n';
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
    dest[dest_index].expected += heap_size;
    /*
      std::cout << "heap " << ph->heap_cnt << " items " << indices[0].value << " " << indices[1].value << " " << indices[2].value << " size " << size
      << " ->"
      << " dest " << dest_index << " indices " << indices[0].index << " " << indices[1].index << " " << indices[2].index
      << " offset " << mem_offset
      << " ntotal " << tstat.ntotal << " noverrun " << tstat.noverrun << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed
      << '\n';
    */
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    et.add_et(et_statistics::ALLOC_TIMING, std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count());
#endif
    return pointer((std::uint8_t*)(mem_base + mem_offset), deleter(shared_from_this(), (void *) std::uintptr_t(size)));
  }

  void allocator::free(std::uint8_t *ptr, void *user)
  {
    (void)ptr;
    (void)user;
  }

  void allocator::show_mark_log()
  {
    if ((tstat.ntotal - log_counter) >= LOG_FREQ)
      {
	std::size_t i;
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
		  << '\n';
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
	      << " cts " << dest[DATA_DEST].cts << " " << dest[TEMP_DEST].cts << '\n';
    //hist.show();
  }

  void allocator::mark(spead2::s_item_pointer_t cnt, bool isok, spead2::s_item_pointer_t reclen)
  {
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
    std::size_t  ctsd, ctst;

    // **** GUARDED BY SEMAPHORE ****
    std::lock_guard<std::mutex> lock(dest_mutex);
    int d = heap2dest[cnt];
    dest[d].needed--;
    dest[d].received += reclen;
    dest[d].cts--;
    heap2dest.erase(cnt);
    tstat.nreceived += reclen;
    //nd = dest[DATA_DEST].needed;
    //nt = dest[TEMP_DEST].needed;
    ctsd = dest[DATA_DEST].cts;
    ctst = dest[TEMP_DEST].cts;
    if (!isok)
      {
	tstat.ndiscarded++;
	dest[d].discarded++;
      }
    else
      {
	tstat.ncompleted++;
	dest[d].completed++;
      }
    if (!opts->quiet) show_mark_log();
    //std::cout << "mark " << cnt << " isok " << isok << " dest " << d << " needed " << nd << " " << nt << " cts " << ctsd << " " << ctst << '\n';
    if ((state == SEQUENTIAL_STATE) && (ctst == 0))
      {
	if (!hasStopped && (dada_mode >= DYNAMIC_DADA_MODE))
	  { // copy the optional side-channel items at the correct position
	    // sci_base = buffer + size - (scape *nsci)
	    std::cout << "STAT "
		      << dest[DATA_DEST].space << " "
		      << dest[DATA_DEST].completed << " "
		      << dest[DATA_DEST].discarded << " "
		      << dest[DATA_DEST].needed << " "
		      << dest[DATA_DEST].expected << " "
		      << dest[DATA_DEST].received
		      << "\n";
            //std::cout << "still needing " << dest[DATA_DEST].needed << " heaps." << '\n';
	    spead2::s_item_pointer_t  *sci_base = (spead2::s_item_pointer_t*)(dest[DATA_DEST].ptr->ptr()
									      + dest[DATA_DEST].size
									      - dest[DATA_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
	    memcpy(sci_base, dest[DATA_DEST].sci, dest[DATA_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
	    memset(dest[DATA_DEST].sci, 0, dest[DATA_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
	    // Release the current slot and get a new one
	    dest[DATA_DEST].ptr->used_bytes(dest[DATA_DEST].ptr->total_bytes());
	    dada.data_stream().release();
	    dest[DATA_DEST].ptr = &dada.data_stream().next();
	    dest[DATA_DEST].completed = 0;
	    dest[DATA_DEST].discarded = 0;
	    dest[DATA_DEST].expected = 0;
	    dest[DATA_DEST].received = 0;
	  }
	if (stop)
	  {
	    hasStopped = true;
	    std::cout << "request to stop the transfer into the ringbuffer received." << '\n';
	    if (dada_mode >= DYNAMIC_DADA_MODE)
	      { // release the previously allocated slot without any data -> used as end signal
		dada.data_stream().release();
	      }
	    stop = false;
            hist.show();
	  }
	dest[DATA_DEST].needed  = dest[DATA_DEST].space;
	indices[0].first  += dest[DATA_DEST].capacity*indices[0].step;
	dest[DATA_DEST].cts = cts_data;
	// switch to parallel data/temp order
	state = PARALLEL_STATE;
	if (!opts->quiet) show_state_log();
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
	    dest[DATA_DEST].completed += dest[TEMP_DEST].completed;
	    dest[DATA_DEST].discarded += dest[TEMP_DEST].discarded;
	    dest[DATA_DEST].expected += dest[TEMP_DEST].expected;
	    dest[DATA_DEST].received += dest[TEMP_DEST].received;
	  }
	dest[TEMP_DEST].needed  = dest[TEMP_DEST].space;
	dest[DATA_DEST].needed -= dest[TEMP_DEST].space;
        dest[TEMP_DEST].completed = 0;
        dest[TEMP_DEST].discarded = 0;
	dest[TEMP_DEST].expected = 0;
	dest[TEMP_DEST].received = 0;
	dest[TEMP_DEST].cts = cts_temp;
	// switch to sequential data/temp order
	state = SEQUENTIAL_STATE;
	if (!opts->quiet) show_state_log();
      }
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    et.add_et(et_statistics::MARK_TIMING, std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count());
#endif
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
