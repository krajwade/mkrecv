
#include "ascii_header.h"

#include "mkrecv_fengine_allocator.h"

namespace mkrecv
{

  fengine_allocator::fengine_allocator(key_t key, std::string mlname, std::shared_ptr<fengine_options> opts) :
    allocator::allocator(key, mlname, opts),
    fopts(opts)
  {
    int i;

    for (i = 0; i < 64; i++) bcount[i] = 0;
    for (i = 0; i < 4096; i++) fcount[i] = 0;
    group_step = fopts->time_step;   // the difference between consecutive timestamps
    freq_first = fopts->freq_first;  // the lowest frequency in all incomming heaps
    freq_step  = fopts->freq_step;   // the difference between consecutive frequencies
    freq_count = fopts->freq_count;  // the number of frequency bands
    feng_first = fopts->feng_first;  // the lowest fengine id
    feng_count = fopts->feng_count;  // the number of fengines
    heap_count = feng_count*freq_count;
  }

  fengine_allocator::~fengine_allocator()
  {
  }

  void fengine_allocator::handle_dummy_heap(std::size_t size, void *hint)
  {
    spead2::recv::packet_header    *ph = (spead2::recv::packet_header*)hint;

    allocator::handle_dummy_heap(size, hint);
    heap2board[ph->heap_cnt] = 0;
    bstat[0].nexpected += size;
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

  int fengine_allocator::handle_data_heap(std::size_t size,
					  void *hint,
					  std::size_t &heap_index)
  {
    spead2::recv::packet_header    *ph = (spead2::recv::packet_header*)hint;
    spead2::recv::pointer_decoder   decoder(ph->heap_address_bits);
    spead2::s_item_pointer_t        timestamp = 0, feng_id = 0, frequency = 0;
    std::size_t                     time_index;
    std::size_t                     feng_index;
    std::size_t                     freq_index;

    // extract some values from the heap
    spead2::item_pointer_t pts = spead2::load_be<spead2::item_pointer_t>(ph->pointers);
    timestamp = decoder.get_immediate(pts);
    spead2::item_pointer_t pbi = spead2::load_be<spead2::item_pointer_t>(ph->pointers + sizeof(spead2::item_pointer_t));
    feng_id = decoder.get_immediate(pbi);
    spead2::item_pointer_t pfi = spead2::load_be<spead2::item_pointer_t>(ph->pointers + 2*sizeof(spead2::item_pointer_t));
    frequency = decoder.get_immediate(pfi);
    // sanity check
    if ((feng_id < 0) || (feng_id > 63))
      {
	std::cout << "bad feng_id:"
		  << " heap " << ph->heap_cnt << " timestamp " << timestamp << " feng_id " << feng_id << " frequency " << frequency
		  << std::endl;
	tstat.nbierror++;
	return TRASH_DEST;
      }
    if ((frequency < 0) || (frequency > 4095))
      {
	std::cout << "bad frequency:"
		  << " heap " << ph->heap_cnt << " timestamp " << timestamp << " feng_id " << feng_id << " frequency " << frequency
		  << std::endl;
	tstat.nfcerror++;
	return TRASH_DEST;
      }
    // update statistics
    bstat[feng_id].ntotal++;
    bcount[feng_id]++;
    fcount[frequency]++;
    heap2board[ph->heap_cnt] = feng_id;
    bstat[feng_id].nexpected += heap_size;
    // update header and first timestamp if in INIT_STATE
    if (state == INIT_STATE)
      {
	group_first = timestamp + 2*group_step;
	if (dada_mode >= 2)
	  {
	    fopts->set_start_time(group_first);
	  }
      }
    // apply filter
    time_index = (timestamp - group_first + group_step/2)/group_step;
    if ((timestamp > group_first) && (time_index >= 4*dest[DATA_DEST].capacity))
      {
	tstat.ntserror++;
	return TRASH_DEST;
      }
    if (timestamp < group_first)
      {
	tstat.nskipped++;
	return TRASH_DEST;
      }
    feng_index = feng_id - feng_first;
    if ((feng_id < feng_first) || (feng_index >= feng_count))
      {
	tstat.nbiskipped++;
	return TRASH_DEST;
      }
    freq_index = (frequency - freq_first + freq_step/2)/freq_step;
    if ((frequency < freq_first) || (freq_index >= freq_count))
      {
	tstat.nfcskipped++;
	return TRASH_DEST;
      }

    // calculate the heap index: Timestamp -> Engine/Board/Antenna -> Frequency -> Time -> Polarization -> Complex number
    heap_index = (time_index*feng_count* + feng_index)*freq_count + freq_index;
    return DATA_DEST;
  }

  void fengine_allocator::mark_heap(spead2::s_item_pointer_t cnt, bool isok, spead2::s_item_pointer_t reclen)
  {
    int b = heap2board[cnt];
    if (b < 0) b = 0;
    if (b > 63) b = 63;
    heap2board.erase(cnt);
    bstat[b].nreceived += reclen;
    if (!isok)
      {
	bstat[b].ndiscarded++;
      }
    else
      {
	bstat[b].ncompleted++;
      }
  }

  void fengine_allocator::mark_log()
  {
    int i;

    if (log_counter > LOG_FREQ) return;
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
