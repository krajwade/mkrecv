#ifndef mkrecv_allocator_h
#define mkrecv_allocator_h

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

namespace mkrecv
{

  static const std::size_t  MAX_DATA_SPACE = 256*1024*1024;
  static const std::size_t  MAX_TEMPORARY_SPACE = 64*4096*128;

  static const int DATA_DEST  = 0;
  static const int TEMP_DEST  = 1;
  static const int TRASH_DEST = 2;

  static const int INIT_STATE       = 0;
  static const int SEQUENTIAL_STATE = 1;
  static const int PARALLEL_STATE   = 2;

  static const int LOG_FREQ = 10000;

  class statistics
  {
  public:
    std::size_t    ntotal = 0;     // number of received heaps (calls of allocate())
    std::size_t    nskipped = 0;   // number of skipped heaps (before first timestamp)
    std::size_t    noverrun = 0;   // number of lost heaps due to overrun
    std::size_t    ncompleted = 0; // number of completed heaps
    std::size_t    ndiscarded = 0; // number of discarded heaps
    std::size_t    nignored = 0;   // number of ignored heaps (id == 1)
    std::size_t    nexpected = 0;  // number of expected payload bytes (ntotal*heapsize)
    std::size_t    nreceived = 0;  // number of received payload bytes
    std::size_t    ntserror = 0;   // number of heaps which have a suspicious timestamp
    std::size_t    nbiskipped = 0; // number of skipped heaps due to board ids outside range
    std::size_t    nbierror = 0;   // number of heaps which have a suspicious board id
    std::size_t    nfcskipped = 0; // number of skipped heaps due to frequency channels outside range
    std::size_t    nfcerror = 0;   // number of heaps which have a suspicious frequency channel number
  };

  /*
   * This allocator uses three memory areas for putting the incomming heaps into. The main
   * memory area is the current ringbuffer slot provided by the PSR_DADA library. Each slot
   * contains a sequence of heaps. A certain number of heaps are grouped together (normally
   * by a common timestamp value). This 
   */

  class allocator : public spead2::memory_allocator
  {
  protected:
    std::shared_ptr<mkrecv::options>   opts = NULL;
    psrdada_cpp::MultiLog              mlog;
    psrdada_cpp::DadaWriteClient       dada;
    psrdada_cpp::RawBytes             *hdr;   // memory to store constant (header) information
    std::shared_ptr<spead2::mmap_allocator>   memallocator;
    std::mutex                         dest_mutex;
    destination                        dest[3];
    std::unordered_map<spead2::s_item_pointer_t, int> heap2dest;
    std::size_t                        payload_size;// size of one packet payload (ph->payload_length
    std::size_t                        heap_size;   // size of a heap in bytes
    std::size_t                        heap_count;  // number of heaps inside one group (-> subclass!)
    std::size_t                        group_first = 0;  // serial number of the first group (needed for index calculation)
    std::size_t                        group_step = 0;   // the serial number difference between consecutive groups
    int                                state = INIT_STATE;
    statistics                         tstat;
    std::size_t                        dada_mode = 4;
    std::size_t                        log_counter = 0;
    bool                               stop = false;
    bool                               hasStopped = false;
    
  public:
    allocator(key_t key, std::string mlname, std::shared_ptr<mkrecv::options> opts);
    ~allocator();
    virtual spead2::memory_allocator::pointer allocate(std::size_t size, void *hint) override;
  private:
    virtual void free(std::uint8_t *ptr, void *user) override;
  public:
    void mark(spead2::s_item_pointer_t cnt, bool isok, spead2::s_item_pointer_t reclen);
    void request_stop();
    bool is_stopped();
  protected:
    void handle_data_full();
    void handle_temp_full();
    virtual void handle_dummy_heap(std::size_t size, void *hint);
    virtual int handle_data_heap(std::size_t size, void *hint, std::size_t &heap_index);
    virtual void mark_heap(spead2::s_item_pointer_t cnt, bool isok, spead2::s_item_pointer_t reclen);
    virtual void mark_log();
  };
  
}

#endif /* mkrecv_allocator_h */