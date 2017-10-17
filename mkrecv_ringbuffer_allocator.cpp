
#include "mkrecv_ringbuffer_allocator.h"

namespace mkrecv
{

ringbuffer_allocator::ringbuffer_allocator(key_t key, std::string mlname, const options &opts) :
  mlog(mlname),
  dada(key, mlog),
  hdr(NULL, 0)
{
  int i;

  no_dada = opts.no_dada;
  hdr = dada.header_stream().next();
  dest[DATA_DEST].set_buffer(dada.data_stream().next(), dada.data_buffer_size());
  dest[TEMP_DEST].allocate_buffer(MAX_TEMPORARY_SPACE);
  dest[TRASH_DEST].allocate_buffer(MAX_TEMPORARY_SPACE);
  freq_first = opts.freq_first;  // the lowest frequency in all incomming heaps
  freq_step  = opts.freq_step;   // the difference between consecutive frequencies
  freq_count = opts.freq_count;  // the number of frequency bands
  feng_first = opts.feng_first;  // the lowest fengine id
  feng_count = opts.feng_count;  // the number of fengines
  time_step  = opts.time_step;   // the difference between consecutive timestamps
}

ringbuffer_allocator::~ringbuffer_allocator()
{
  delete dest[DATA_DEST].ptr.ptr();
  delete dest[TRASH_DEST].ptr.ptr();
}

spead2::memory_allocator::pointer ringbuffer_allocator::allocate(std::size_t size, void *hint)
{
  spead2::recv::packet_header    *ph = (spead2::recv::packet_header*)hint;
  spead2::recv::pointer_decoder   decoder(ph->heap_address_bits);
  spead2::s_item_pointer_t        timestamp, feng_id, frequency;
  std::size_t                     time_index;
  std::size_t                     freq_index;
  std::size_t                     feng_index;
  char*                           mem_base = NULL;
  std::size_t                     mem_offset;
  int                             d;

  // extract some values from the heap
  for (int i = 0; i < ph->n_items; i++)
    {
      spead2::item_pointer_t pointer = spead2::load_be<spead2::item_pointer_t>(ph->pointers + i * sizeof(spead2::item_pointer_t));
      bool special;
      if (decoder.is_immediate(pointer))
        {
	  switch (decoder.get_id(pointer))
            {
            case TIMESTAMP_ID:
	      timestamp = decoder.get_immediate(pointer);
	      break;
            case FENG_ID_ID:
	      feng_id = decoder.get_immediate(pointer);
	      break;
            case FREQUENCY_ID:
	      frequency = decoder.get_immediate(pointer);
	      break;
            default:
	      break;
            }
        }
    }
  //std::cout << "heap " << ph->heap_cnt << " timestamp " << timestamp << " feng_id " << feng_id << " frequency " << frequency << std::endl;
  // put some values in the header buffer
  ntotal++;
  if (state == INIT_STATE)
    { // put some values in the header buffer
      header  h;
      h.timestamp = timestamp + 2*time_step;
      memcpy(hdr.ptr(), &h, sizeof(h));
      hdr.used_bytes(sizeof(h));
      dada.header_stream().release();
      // calculate some values needed to calculate a memory offset for incoming heaps
      heap_size = size;
      freq_size = heap_size;
      feng_size = freq_count*freq_size;
      time_size = feng_count*feng_size;
      dest[DATA_DEST].capacity  = dest[DATA_DEST].size/time_size;
      dest[TEMP_DEST].capacity  = dest[TEMP_DEST].size/time_size;
      dest[TRASH_DEST].capacity = dest[TRASH_DEST].size/time_size;
      dest[DATA_DEST].first   = timestamp + 2*time_step;
      dest[DATA_DEST].space   = dest[DATA_DEST].capacity*feng_count*freq_count;
      dest[DATA_DEST].needed  = dest[DATA_DEST].space;
      dest[TEMP_DEST].space   = dest[TEMP_DEST].capacity*feng_count*freq_count;
      dest[TEMP_DEST].needed  = dest[TEMP_DEST].space;
      dest[TRASH_DEST].space  = dest[TRASH_DEST].capacity*feng_count*freq_count;
      dest[TRASH_DEST].needed = dest[TRASH_DEST].space;
      std::cout << "sizes: heap " << heap_size << " freq " << freq_size << " feng " << feng_size << " time " << time_size << std::endl;
      std::cout << "DATA_DEST:  capacity " << dest[DATA_DEST].capacity << " space " << dest[DATA_DEST].space << " first " << dest[DATA_DEST].first << std::endl;
      std::cout << "TEMP_DEST:  capacity " << dest[TEMP_DEST].capacity << " space " << dest[TEMP_DEST].space << " first " << dest[TEMP_DEST].first << std::endl;
      std::cout << "TRASH_DEST: capacity " << dest[TRASH_DEST].capacity << " space " << dest[TRASH_DEST].space << " first " << dest[TRASH_DEST].first << std::endl;
      state = SEQUENTIAL_STATE;
    }
  // The term "+ time_step/2" allows to have nonintegral time_steps.
  // The term "+ freq_step/2" allows to have nonintegral freq_steps.
  time_index = (timestamp - dest[DATA_DEST].first + time_step/2)/time_step;
  feng_index = feng_id - feng_first;
  freq_index = (frequency - freq_first + freq_step/2)/freq_step;
  if ((time_index < 0) || (feng_index < 0) || (feng_index >= feng_count) || (freq_index < 0) || (freq_index >= freq_count))
    { // at least one index out of range -> unwanted heaps will go into thrash
      d = TRASH_DEST;
      time_index = 0;
    }
  else if (((state == SEQUENTIAL_STATE) && (time_index >= dest[DATA_DEST].capacity + dest[TEMP_DEST].capacity))
	   ||
	   ((state == PARALLEL_STATE) && (time_index >= dest[DATA_DEST].capacity)))
    { // we are not fast enough to keep up with the data stream -> lost heaps will go into trash
      d = TRASH_DEST;
      time_index = 0;
      noverrun++;
    }
  else if (state == SEQUENTIAL_STATE)
    { // data and temp are in sequential order
      if (time_index < dest[DATA_DEST].capacity)
	{ // the current heap will go into the ringbuffer slot
	  d = DATA_DEST;
	}
      else
	{ // the current heap will go into the temporary memory
	  // determine the first timestamp in the temporary buffer
	  if (dest[TEMP_DEST].first == 0) dest[TEMP_DEST].first = timestamp;
	  if (dest[TEMP_DEST].first > timestamp) dest[TEMP_DEST].first = timestamp;
	  d = TEMP_DEST;
	  time_index -= dest[DATA_DEST].capacity;
	}
    }
  else if (state == PARALLEL_STATE)
    { // data and temp are in parallel order
      if (time_index < dest[TEMP_DEST].capacity)
	{ // the current heap will go into the temporary buffer until it is full and is copied at the beginning of the data buffer
	  d = TEMP_DEST;
	}
      else
	{ // the current heap will go into the ringbuffer slot
	  d = DATA_DEST;
	}
    }
  mem_base   = dest[d].ptr.ptr();
  mem_offset = time_size*time_index;
  // add the remaining offsets
  // The term "+ freq_step/2" allows to have nonintegral freq_step.
  mem_offset += feng_size*feng_index;
  mem_offset += freq_size*freq_index;
  if (no_dada)
    {
      d = TRASH_DEST;
    }
  if (d == TRASH_DEST)
    {
      mem_offset = 0;
    }
  dest[d].count++;
  heap2dest[ph->heap_cnt] = d; // store the relation between heap counter and destination
  /*
  std::cout << "heap " << ph->heap_cnt << " timestamp " << timestamp << " feng_id " << feng_id << " frequency " << frequency << " size " << size
  	    << " ->"
	    << " dest " << d << " indices " << time_index << " " << feng_index << " " << freq_index
	    << " offset " << mem_offset
	    << " nlost " << nlost << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed
	    << std::endl;
  */
  /*
  d = TRASH_DEST;
  heap2dest[ph->heap_cnt] = d;
  mem_base = dest[d].ptr.ptr();
  mem_offset = 0;
  */
  return pointer((std::uint8_t*)(mem_base + mem_offset), deleter(shared_from_this(), (void *) std::uintptr_t(size)));
}

void ringbuffer_allocator::free(std::uint8_t *ptr, void *user)
{
}

void ringbuffer_allocator::handle_data_full()
{
  std::lock_guard<std::mutex> lock(dest_mutex);
  std::cout << "-> parallel total " << ntotal << " completed " << ncompleted << " discarded " << ndiscarded << std::endl;
  // release the current ringbuffer slot
  dada.data_stream().release();	
  // get a new ringbuffer slot
  dest[DATA_DEST].ptr     = dada.data_stream().next();
  dest[DATA_DEST].needed  = dest[DATA_DEST].space;
  dest[DATA_DEST].first  += dest[DATA_DEST].capacity*time_step;
  // switch to parallel data/temp order
  state = PARALLEL_STATE;
}

void ringbuffer_allocator::handle_temp_full()
{
  memcpy(dest[DATA_DEST].ptr.ptr(), dest[TEMP_DEST].ptr.ptr(), dest[TEMP_DEST].space*heap_size);
  std::lock_guard<std::mutex> lock(dest_mutex);
  std::cout << "-> sequential total " << ntotal << " completed " << ncompleted << " discarded " << ndiscarded << std::endl;
  dest[TEMP_DEST].needed  = dest[TEMP_DEST].space;
  dest[TEMP_DEST].first   = 0;
  dest[DATA_DEST].needed -= dest[TEMP_DEST].space;
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

void ringbuffer_allocator::mark(spead2::s_item_pointer_t cnt, bool isok)
{
  std::size_t  nd, nt;

  {
    std::lock_guard<std::mutex> lock(dest_mutex);
    int d = heap2dest[cnt];
    dest[d].needed--;
    heap2dest.erase(cnt);
    nd = dest[DATA_DEST].needed;
    nt = dest[TEMP_DEST].needed;
    if (!isok)
      {
        ndiscarded++;
      }
    else
      {
	ncompleted++;
      }
    //std::cout << "mark " << cnt << " isok " << isok << " dest " << d << " needed " << nd << " " << nt << std::endl;
    if (ntotal%1024 == 0)
      {
        std::cout << "heaps: total " << ntotal << " completed " << ncompleted << " discarded " << ndiscarded << " overrun " << noverrun
	       	  << " assigned " << dest[DATA_DEST].count << " " << dest[TEMP_DEST].count << " " << dest[TRASH_DEST].count
		  << " needed " << dest[DATA_DEST].needed << " " << dest[TEMP_DEST].needed << " " << dest[TRASH_DEST].needed
		  << std::endl;
      }
    if (nd > 100000)
      {
	std::cout << "mark " << cnt << " isok " << isok << " dest " << d << " needed " << nd << " " << nt << std::endl;
        exit(0);
      }
  }
  if (nd == 0)
    {
      handle_data_full(); // std::thread dfull(do_handle_data_full, this); <- does not work, terminate
    }
  else if (nt == 0)
    {
      handle_temp_full(); // std::thread tfull(do_handle_temp_full, this); <- does not work, terminate
    }
}

void ringbuffer_allocator::mark(spead2::recv::heap &heap)
{
  mark(heap.get_cnt(), true);
}


}
