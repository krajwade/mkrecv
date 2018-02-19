
#include "ascii_header.h"

#include "mkrecv_ringbuffer_allocator.h"

namespace mkrecv
{

ringbuffer_allocator::ringbuffer_allocator(key_t key, std::string mlname, options *opts) :
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
  tstat.ntotal = 0;
  tstat.nskipped = 0;
  tstat.noverrun = 0;
  tstat.ncompleted = 0;
  tstat.ndiscarded = 0;
  tstat.nignored = 0;
  tstat.nexpected = 0;
  tstat.nreceived = 0;
  tstat.ntserror = 0;
  tstat.nbiskipped = 0;
  tstat.nbierror = 0;
  tstat.nfcskipped = 0;
  tstat.nfcerror = 0;
  for (i = 0; i < 64; i++)
    {
      bstat[i].ntotal = 0;
      bstat[i].nskipped = 0;
      bstat[i].noverrun = 0;
      bstat[i].ncompleted = 0;
      bstat[i].ndiscarded = 0;
      bstat[i].nignored = 0;
      bstat[i].nexpected = 0;
      bstat[i].nreceived = 0;
      bstat[i].ntserror = 0;
      bstat[i].nbiskipped = 0;
      bstat[i].nbierror = 0;
      bstat[i].nfcskipped = 0;
      bstat[i].nfcerror = 0;
    }
  for (i = 0; i < 64; i++) bcount[i] = 0;
  for (i = 0; i < 4096; i++) fcount[i] = 0;
  dest[TEMP_DEST].allocate_buffer(memallocator, MAX_TEMPORARY_SPACE);
  dest[TRASH_DEST].allocate_buffer(memallocator, MAX_TEMPORARY_SPACE);
  std::cout << "dest[DATA_DEST].ptr.ptr()  = " << (std::size_t)(dest[DATA_DEST].ptr->ptr()) << std::endl;
  std::cout << "dest[TEMP_DEST].ptr.ptr()  = " << (std::size_t)(dest[TEMP_DEST].ptr->ptr()) << std::endl;
  std::cout << "dest[TRASH_DEST].ptr.ptr() = " << (std::size_t)(dest[TRASH_DEST].ptr->ptr()) << std::endl;
  freq_first = opts->freq_first;  // the lowest frequency in all incomming heaps
  freq_step  = opts->freq_step;   // the difference between consecutive frequencies
  freq_count = opts->freq_count;  // the number of frequency bands
  feng_first = opts->feng_first;  // the lowest fengine id
  feng_count = opts->feng_count;  // the number of fengines
  time_step  = opts->time_step;   // the difference between consecutive timestamps
}

ringbuffer_allocator::~ringbuffer_allocator()
{
  if (dada_mode <= 1)
    {
      delete dest[DATA_DEST].ptr->ptr();
      delete dest[DATA_DEST].ptr;
    }
  delete dest[TEMP_DEST].ptr->ptr();
  delete dest[TEMP_DEST].ptr;
  delete dest[TRASH_DEST].ptr->ptr();
  delete dest[TRASH_DEST].ptr;
}

spead2::memory_allocator::pointer ringbuffer_allocator::allocate(std::size_t size, void *hint)
{
  spead2::recv::packet_header    *ph = (spead2::recv::packet_header*)hint;
  spead2::recv::pointer_decoder   decoder(ph->heap_address_bits);
  spead2::s_item_pointer_t        timestamp = 0, feng_id = 0, frequency = 0, otimestamp = 0, ofeng_id = 0, ofrequency = 0;
  int                             its = 0, ibi = 0, ifi = 0;
  std::size_t                     time_index;
  std::size_t                     freq_index;
  std::size_t                     feng_index;
  char*                           mem_base = NULL;
  std::size_t                     mem_offset;
  int                             d;
  int                             isbad = 0;

  std::lock_guard<std::mutex> lock(dest_mutex);
  if (ph->heap_cnt == 1)
    {
      d = TRASH_DEST;
      tstat.ntotal++;
      tstat.nignored++;
      dest[d].count++;
      heap2dest[ph->heap_cnt] = d;
      heap2board[ph->heap_cnt] = feng_id;
      tstat.nexpected += heap_size;
      bstat[feng_id].nexpected += heap_size;
      return pointer((std::uint8_t*)(dest[d].ptr->ptr()), deleter(shared_from_this(), (void *)std::uintptr_t(size)));
    }
  // extract some values from the heap
  /*
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
  spead2::item_pointer_t pts = spead2::load_be<spead2::item_pointer_t>(ph->pointers);
  otimestamp = timestamp = decoder.get_immediate(pts);
  spead2::item_pointer_t pbi = spead2::load_be<spead2::item_pointer_t>(ph->pointers + sizeof(spead2::item_pointer_t));
  ofeng_id = feng_id = decoder.get_immediate(pbi);
  spead2::item_pointer_t pfi = spead2::load_be<spead2::item_pointer_t>(ph->pointers + 2*sizeof(spead2::item_pointer_t));
  ofrequency = frequency = decoder.get_immediate(pfi);
  //std::cout << "heap " << ph->heap_cnt << " timestamp " << timestamp << " feng_id " << feng_id << " frequency " << frequency << std::endl;
  if ((ofeng_id < 0) || (ofeng_id > 63))
    {
      std::cout << "heap " << ph->heap_cnt << " timestamp " << timestamp << " feng_id " << feng_id << " frequency " << frequency << std::endl;
      feng_id = 0;
      isbad = 1;
    }
  // put some values in the header buffer
  tstat.ntotal++;
  bstat[feng_id].ntotal++;
  bcount[feng_id]++;
  fcount[frequency]++;
  if (state == INIT_STATE)
    { // put some values in the header buffer
      if (dada_mode >= 2)
	{
	  /*
          header  h;
          h.timestamp = timestamp + 2*time_step;
          memcpy(hdr->ptr(), &h, sizeof(h));
          hdr->used_bytes(sizeof(h));
          dada.header_stream().release();
	  hdr = NULL;
	  */
	  opts->set_start_time(timestamp + 2*time_step);
	  /*
	  ascii_header_set(opts->header, SOURCES_KEY, "%s", opts->used_sources.c_str());
	  opts->check_header();
	  */
          memcpy(hdr->ptr(), opts->header, (DADA_DEFAULT_HEADER_SIZE < hdr->total_bytes()) ? DADA_DEFAULT_HEADER_SIZE : hdr->total_bytes());
          hdr->used_bytes(hdr->total_bytes());
          dada.header_stream().release();
	  hdr = NULL;
	}
      // calculate some values needed to calculate a memory offset for incoming heaps
      payload_size = ph->payload_length;
      heap_size = size;
      freq_size = heap_size;
      feng_size = freq_count*freq_size;
      time_size = feng_count*feng_size;
      dest[DATA_DEST].capacity  = dest[DATA_DEST].size/time_size;
      dest[TEMP_DEST].capacity  = 2; //dest[TEMP_DEST].size/time_size;
      dest[TRASH_DEST].capacity = 2; //dest[TRASH_DEST].size/time_size;
      dest[DATA_DEST].first   = timestamp + 2*time_step;
      dest[DATA_DEST].space   = dest[DATA_DEST].capacity*feng_count*freq_count;
      dest[DATA_DEST].needed  = dest[DATA_DEST].space;
      dest[TEMP_DEST].space   = dest[TEMP_DEST].capacity*feng_count*freq_count;
      dest[TEMP_DEST].needed  = dest[TEMP_DEST].space;
      dest[TRASH_DEST].space  = dest[TRASH_DEST].capacity*feng_count*freq_count;
      dest[TRASH_DEST].needed = dest[TRASH_DEST].space;
      dest[TEMP_DEST].cts = 1;
      std::cout << "sizes: heap " << heap_size << " freq " << freq_size << " feng " << feng_size << " time " << time_size << std::endl;
      std::cout << "DATA_DEST:  capacity " << dest[DATA_DEST].capacity << " space " << dest[DATA_DEST].space << " first " << dest[DATA_DEST].first << std::endl;
      std::cout << "TEMP_DEST:  capacity " << dest[TEMP_DEST].capacity << " space " << dest[TEMP_DEST].space << " first " << dest[TEMP_DEST].first << std::endl;
      std::cout << "TRASH_DEST: capacity " << dest[TRASH_DEST].capacity << " space " << dest[TRASH_DEST].space << " first " << dest[TRASH_DEST].first << std::endl;
      std::cout << "heap " << ph->heap_cnt << " timestamp " << otimestamp << " feng_id " << ofeng_id << " frequency " << ofrequency << std::endl;
      std::cout << "indices: timestamp " << its << " board id " << ibi << " frequency channel " << ifi << std::endl;
      state = SEQUENTIAL_STATE;
    }
  // The term "+ time_step/2" allows to have nonintegral time_steps.
  // The term "+ freq_step/2" allows to have nonintegral freq_steps.
  time_index = (timestamp - dest[DATA_DEST].first + time_step/2)/time_step;
  feng_index = feng_id - feng_first;
  freq_index = (frequency - freq_first + freq_step/2)/freq_step;
  if (hasStopped)
    {
      tstat.nskipped++;
      d = TRASH_DEST;
    }
  else if (timestamp < dest[DATA_DEST].first)
    {
      tstat.nskipped++;
      d = TRASH_DEST;
    }
  else if ((timestamp > dest[DATA_DEST].first) && (time_index >= 4*dest[DATA_DEST].capacity))
    {
      tstat.ntserror++;
      d = TRASH_DEST;
      isbad = 1;
    }
  /*
  else if (ofeng_id >= 64)
    {
      tstat.nbierror++;
      d = TRASH_DEST;
      isbad = 1;
    }
  */
  else if ((feng_id < feng_first) || (feng_index >= feng_count))
    {
      tstat.nbiskipped++;
      d = TRASH_DEST;
    }
  /*
  else if (ofrequency >= 4096)
    {
      tstat.nfcerror++;
      d = TRASH_DEST;
      isbad = 1;
    }
  */
  else if ((frequency < freq_first) || (freq_index >= freq_count))
    {
      tstat.nfcskipped++;
      d = TRASH_DEST;
    }
  else if (state == SEQUENTIAL_STATE)
    {
      if (time_index >= (dest[DATA_DEST].capacity + dest[TEMP_DEST].capacity))
        {
          tstat.noverrun++;
          d = TRASH_DEST;
        }
      else if (time_index >= dest[DATA_DEST].capacity)
        {
          d = TEMP_DEST;
          time_index -= dest[DATA_DEST].capacity;
        }
      else
        {
          d = DATA_DEST;
        }
    }
  else if (state == PARALLEL_STATE)
    {
      if (time_index >= dest[DATA_DEST].capacity)
        {
          tstat.noverrun++;
          d = TRASH_DEST;
        }
      else if (time_index >= dest[TEMP_DEST].capacity)
        {
          d = DATA_DEST;
        }
      else
        {
          d = TEMP_DEST;
        }
    }
  if (isbad)
    {
      //std::cout << "error heap: " << ph->heap_cnt << " timestamp " << otimestamp << " feng_id " << ofeng_id << " frequency " << ofrequency << " indices " << time_index << " " << feng_index << " " << freq_index << std::endl;
    }
  if (dada_mode == 0)
    {
      d = TRASH_DEST;
    }
  if (d == TRASH_DEST)
    {
      time_index = 0;
    }
  mem_offset = time_size*time_index + feng_size*feng_index + freq_size*freq_index;
  mem_base = dest[d].ptr->ptr();
  dest[d].count++;
  heap2dest[ph->heap_cnt] = d; // store the relation between heap counter and destination
  heap2board[ph->heap_cnt] = feng_id;
  tstat.nexpected += heap_size;
  bstat[feng_id].nexpected += heap_size;
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

void ringbuffer_allocator::free(std::uint8_t *ptr, void *user)
{
}

void ringbuffer_allocator::handle_data_full()
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
  dest[DATA_DEST].first  += dest[DATA_DEST].capacity*time_step;
  dest[DATA_DEST].cts = freq_count*feng_count;
  /*
  std::cout << "-> parallel total " << tstat.ntotal << " completed " << tstat.ncompleted << " discarded " << tstat.ndiscarded << " skipped " << tstat.nskipped << " overrun " << tstat.noverrun
	    << " assigned " << dest[DATA_DEST].count << " " << dest[TEMP_DEST].count << " " << dest[TRASH_DEST].count
	    << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed
	    << " payload " << tstat.nexpected << " " << tstat.nreceived << std::endl;
  */
  // switch to parallel data/temp order
  state = PARALLEL_STATE;
}

void ringbuffer_allocator::handle_temp_full()
{
  if ((dada_mode >= 4) && !hasStopped)
    {
      memcpy(dest[DATA_DEST].ptr->ptr(), dest[TEMP_DEST].ptr->ptr(), dest[TEMP_DEST].space*heap_size);
    }
  //std::lock_guard<std::mutex> lock(dest_mutex);
  dest[TEMP_DEST].needed  = dest[TEMP_DEST].space;
  dest[TEMP_DEST].first   = 0;
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

void do_handle_data_full(ringbuffer_allocator *rb)
{
  rb->handle_data_full();
}

void do_handle_temp_full(ringbuffer_allocator *rb)
{
  rb->handle_temp_full();
}

void ringbuffer_allocator::mark(spead2::s_item_pointer_t cnt, bool isok, spead2::s_item_pointer_t reclen)
{
  std::size_t  nd, nt, rt, ctsd, ctst;

  {
    std::lock_guard<std::mutex> lock(dest_mutex);
    int d = heap2dest[cnt];
    int b = heap2board[cnt];
    if (b < 0) b = 0;
    if (b > 63) b = 63;
    dest[d].needed--;
    dest[d].cts--;
    heap2dest.erase(cnt);
    heap2board.erase(cnt);
    tstat.nreceived += reclen;
    bstat[b].nreceived += reclen;
    nd = dest[DATA_DEST].needed;
    nt = dest[TEMP_DEST].needed;
    rt = dest[TEMP_DEST].space - dest[TEMP_DEST].needed;
    ctsd = dest[DATA_DEST].cts;
    ctst = dest[TEMP_DEST].cts;
    if (!isok)
      {
        tstat.ndiscarded++;
	bstat[b].ndiscarded++;
      }
    else
      {
	tstat.ncompleted++;
	bstat[b].ncompleted++;
      }
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
	/*
	for (i = 0; i < 64; i++)
	  {
            if (bstat[i].nexpected == 0) continue;
	    std::cout << "   board " << i
		      << " total " << bstat[i].ntotal << " completed " << bstat[i].ncompleted << " discarded " << bstat[i].ndiscarded << " overrun " << bstat[i].noverrun
                      << " payload " << bstat[i].nexpected << " " << bstat[i].nreceived
		      << std::endl;
          }
	*/
	if (log_counter <= LOG_FREQ)
	  {
	    for (i = 0; i < 64; i++)
	      {
	        if (bcount[i] == 0) continue;
		std::cout << "board id " << i << " total " << bcount[i] << std::endl;
	      }
	    for (i = 0; i < 4096; i++)
	      {
		if (fcount[i] == 0) continue;
		std::cout << "channel " << i << " total " << fcount[i] << std::endl;
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

void ringbuffer_allocator::requestStop()
{
  stop = true;
}

bool ringbuffer_allocator::isStopped()
{
  return hasStopped;
}

}
