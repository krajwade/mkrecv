#ifndef mkrecv_ringbuffer_allocator_h
#define mkrecv_ringbuffer_allocator_h

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

  static const unsigned int FENG_ID_ID     = 0x4101;
  static const unsigned int FREQUENCY_ID   = 0x4103;
  static const unsigned int FENG_RAW_ID    = 0x4300;

  class ringbuffer_allocator : public mkrecv::allocator
  {
  private:
    fengine_options              *fopts;
    /// Mutex protecting @ref allocator
    std::unordered_map<spead2::s_item_pointer_t, int> heap2board;
    std::size_t                   freq_size;   //
    std::size_t                   freq_first;  // the lowest frequency in all incomming heaps
    std::size_t                   freq_step;   // the difference between consecutive frequencies
    std::size_t                   freq_count;  // the number of frequency bands
    std::size_t                   feng_size;   // freq_count*freq_size
    std::size_t                   feng_first;  // the lowest fengine id
    std::size_t                   feng_count;  // the number of fengines
    statistics_t                  bstat[64];
    std::size_t                   bcount[64];
    std::size_t                   fcount[4096];
    
  public:
    ringbuffer_allocator(key_t key, std::string mlname, mkrecv::fengine_options *opts);
    ~ringbuffer_allocator();
    virtual spead2::memory_allocator::pointer allocate(std::size_t size, void *hint) override;
    
  private:
    virtual void free(std::uint8_t *ptr, void *user) override;
    
  public:
    void handle_data_full();
    void handle_temp_full();
    void mark(spead2::s_item_pointer_t cnt, bool isok, spead2::s_item_pointer_t reclen);
  };
  
}

#endif /* mkrecv_ringbuffer_allocator_h */
