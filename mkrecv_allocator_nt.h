#ifndef mkrecv_allocator_nt_h
#define mkrecv_allocator_nt_h

#include <cstdint>
//#include <unordered_map>

#include <spead2/common_thread_pool.h>
#include <spead2/recv_udp.h>
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

#include "mkrecv_options.h"
#include "mkrecv_destination.h"
#include "mkrecv_allocator.h"
#include "mkrecv_storage.h"

namespace mkrecv
{

  /*
   * This allocator uses three memory areas for putting the incomming heaps into. The main
   * memory area is the current ringbuffer slot provided by the PSR_DADA library. Each slot
   * contains a sequence of heaps. A certain number of heaps are grouped together (normally
   * by a common timestamp value). This 
   */

  class allocator_nt : public spead2::memory_allocator
  {
  protected:
    static const int MAX_OPEN_HEAPS  = 4;
  protected:
    std::shared_ptr<mkrecv::options>                                         opts;
    std::shared_ptr<mkrecv::storage>                                         store;
    //std::unordered_map<spead2::s_item_pointer_t, int>                        heap2dest;
    //std::unordered_map<spead2::s_item_pointer_t, spead2::s_item_pointer_t>   heap2timestamp;
    std::size_t                                                              head = 0;
    std::size_t                                                              tail = 0;
    spead2::s_item_pointer_t                                                 heap_id[MAX_OPEN_HEAPS];
    int                                                                      heap_dest[MAX_OPEN_HEAPS];
    spead2::s_item_pointer_t                                                 heap_timestamp[MAX_OPEN_HEAPS];
    std::size_t                                                              nindices = 0;
    index_part                                                               indices[MAX_INDEXPARTS];
    ts_histo                                                                 hist;
    std::size_t                                                              payload_size;// size of one packet payload (ph->payload_length
    std::size_t                                                              heap_size;   // size of a heap in bytes
    std::size_t                                                              heap_count;  // number of heaps inside one group
    std::size_t                                                              nsci;
    std::vector<std::size_t>                                                 scis;
    bool                                                                     has_started = false;
    bool                                                                     stop = false;
    bool                                                                     has_stopped = false;
    std::size_t                                                              heaps_total = 0;
    et_statistics                                                            et;

  public:
    allocator_nt(std::shared_ptr<mkrecv::options> opts, std::shared_ptr<mkrecv::storage> store);
    ~allocator_nt();
    virtual spead2::memory_allocator::pointer allocate(std::size_t size, void *hint) override;
  public:
    void mark(spead2::s_item_pointer_t cnt, bool isok, spead2::s_item_pointer_t reclen);
    void request_stop();
    bool is_stopped();
  private:
    virtual void free(std::uint8_t *ptr, void *user) override;
  };
  
}

#endif /* mkrecv_allocator_nt_h */
