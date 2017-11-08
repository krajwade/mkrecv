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

#include "mkrecv_options.h"
#include "mkrecv_destination.h"

namespace mkrecv
{

  static const unsigned int TIMESTAMP_ID   = 0x1600;
  static const unsigned int FENG_ID_ID     = 0x4101;
  static const unsigned int FREQUENCY_ID   = 0x4103;
  static const unsigned int FENG_RAW_ID    = 0x4300;

  static const std::size_t  MAX_DATA_SPACE = 256*1024*1024;
  static const std::size_t  MAX_TEMPORARY_SPACE = 64*4096*128;

  static const int DATA_DEST  = 0;
  static const int TEMP_DEST  = 1;
  static const int TRASH_DEST = 2;

  static const int INIT_STATE       = 0;
  static const int SEQUENTIAL_STATE = 1;
  static const int PARALLEL_STATE   = 2;

  static const int LOG_FREQ = 10000;

  struct header
  {
    spead2::s_item_pointer_t   timestamp;
  };

  typedef struct 
  {
    std::size_t    ntotal;     // number of received heaps (calls of allocate())
    std::size_t    nskipped;   // number of skipped heaps (before first timestamp)
    std::size_t    noverrun;   // number of lost heaps due to overrun
    std::size_t    ncompleted; // number of completed heaps
    std::size_t    ndiscarded; // number of discarded heaps
    std::size_t    nignored;   // number of ignored heaps (id == 1)
    std::size_t    nexpected;  // number of expected payload bytes (ntotal*heapsize)
    std::size_t    nreceived;  // number of received payload bytes
    std::size_t    ntserror;   // number of heaps which have a suspicious timestamp
    std::size_t    nbiskipped; // number of skipped heaps due to board ids outside range
    std::size_t    nbierror;   // number of heaps which have a suspicious board id
    std::size_t    nfcskipped; // number of skipped heaps due to frequency channels outside range
    std::size_t    nfcerror;   // number of heaps which have a suspicious frequency channel number
  } statistics_t;

  class ringbuffer_allocator : public spead2::memory_allocator
  {
  private:
    /// Mutex protecting @ref allocator
    psrdada_cpp::MultiLog         mlog;
    psrdada_cpp::DadaWriteClient  dada;
    psrdada_cpp::RawBytes         hdr;   // memory to store constant (header) information
    std::shared_ptr<spead2::mmap_allocator>   memallocator;
    std::mutex                    dest_mutex;
    destination                   dest[3];
    std::unordered_map<spead2::s_item_pointer_t, int> heap2dest;
    std::unordered_map<spead2::s_item_pointer_t, int> heap2board;
    std::size_t                   payload_size;// size of one packet payload (ph->payload_length
    std::size_t                   heap_size;   // Size of a complete HEAP
    std::size_t                   freq_size;   //
    std::size_t                   freq_first;  // the lowest frequency in all incomming heaps
    std::size_t                   freq_step;   // the difference between consecutive frequencies
    std::size_t                   freq_count;  // the number of frequency bands
    std::size_t                   feng_size;   // freq_count*freq_size
    std::size_t                   feng_first;  // the lowest fengine id
    std::size_t                   feng_count;  // the number of fengines
    std::size_t                   time_size;   // feng_count*feng_size
    std::size_t                   time_step;   // the difference between consecutive timestamps
    int                           state = INIT_STATE;
    statistics_t                  tstat;
    statistics_t                  bstat[64];
    std::size_t                   dada_mode = 4;
    std::size_t                   log_counter = 0;
    
  public:
    ringbuffer_allocator(key_t key, std::string mlname, const mkrecv::options &opts);
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
