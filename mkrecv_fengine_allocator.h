#ifndef mkrecv_fengine_allocator_h
#define mkrecv_fengine_allocator_h

#include <cstdint>
#include <unordered_map>

#include <spead2/common_thread_pool.h>
#include <spead2/recv_udp.h>
#if SPEAD2_USE_NETMAP
# include <spead2/recv_netmap.h>
#endif
#if SPEAD2_USE_IBV
# include <spead2/recv_udp_ibv.h>
#endif
#include <spead2/recv_heap.h>
#include <spead2/recv_live_heap.h>
#include <spead2/recv_ring_stream.h>
#include <spead2/recv_packet.h>
#include <spead2/recv_utils.h>
#include <spead2/common_defines.h>
#include <spead2/common_logging.h>
#include <spead2/common_endian.h>

#include "psrdada_cpp/dada_write_client.hpp"

#include "mkrecv_fengine_options.h"
#include "mkrecv_destination.h"

#include "mkrecv_allocator.h"

namespace mkrecv
{

  //static const unsigned int TIMESTAMP_ID   = 0x1600;
  //static const unsigned int FENG_ID_ID     = 0x4101;
  //static const unsigned int FREQUENCY_ID   = 0x4103;
  //static const unsigned int FENG_RAW_ID    = 0x4300;

  class fengine_allocator : public mkrecv::allocator
  {
  protected:
    //std::unordered_map<spead2::s_item_pointer_t, int> heap2board;
    //std::size_t                        freq_first;  // the lowest frequency in all incomming heaps
    //std::size_t                        freq_step;   // the difference between consecutive frequencies
    //std::size_t                        freq_count;  // the number of frequency bands
    //std::size_t                        feng_first;  // the lowest fengine id
    //std::size_t                        feng_count;  // the number of fengines
    statistics                         bstat[64];
    
  public:
    fengine_allocator(key_t key, std::string mlname, std::shared_ptr<mkrecv::options> opts);
    ~fengine_allocator();
  protected:
    int handle_data_heap(std::size_t size, void *hint, std::size_t &heap_index);
  };
  
}

#endif /* mkrecv_fengine_allocator_h */
