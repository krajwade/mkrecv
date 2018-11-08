#ifndef mkrecv_allocator_h
#define mkrecv_allocator_h

#include <cstdint>
#include <unordered_map>

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

  static const int LOG_FREQ = 100000;

  static const std::size_t MAX_VALUE = 4096;

  static const int TS_HISTO_SLOTS = 8;

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
    std::size_t    nerror = 0;     // number of heaps which have a suspicious item pointer value
  };

  class index_part : public index_options
  {
  public:
    std::unordered_map<spead2::s_item_pointer_t, std::size_t> value2index;
    spead2::s_item_pointer_t  first    = 0;
    std::size_t               count    = 0;
    std::size_t               nerror   = 0;
    std::size_t               nskipped = 0;
    index_part();
    void set(const index_options &opt);
  };

  class ts_histo
  {
  public:
    bool                       has_last_ts;
    std::int64_t               ts_step;
    std::int64_t               last_ts;
    std::int64_t               most_negative_index;
    std::size_t                count_negative_indices;
    std::size_t                counts[2*TS_HISTO_SLOTS + 1];
    std::int64_t               most_positive_index;
    std::size_t                count_positive_indices;
    ts_histo();
    void set_step(std::int64_t step);
    void proc_ts(std::int64_t ts);
    void show();
  };

  /*
   * histo: histogram with logarithmic slots:
   * 10, 20, 50 ns
   * 100, 200, 500 ns
   * 1, 2, 5 us
   * 10, 20, 50 us
   * 100, 200, 500 us
   * 1, 2, 5 ms
   * 10, 20, 50 ms
   * 100, 200, 500 ms
   */
  class et_statistics
  {
  public:
    static const int ALLOC_TIMING = 0;
    static const int MARK_TIMING  = 1;
    static const int MAX_SLOTS    = 8*3+1;
    static const int LARGER_SLOT  = MAX_SLOTS-1;
  public:
    double   min_et[2];
    double   max_et[2];
    double   sum_et[2];
    double   count_et[2];
    int      histo[2][MAX_SLOTS]; // 8 times 1,2,5
  public:
    et_statistics();
    void add_et(int fi, double nanoseconds);
    void show();
    void reset();
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
    std::size_t                        nindices = 0;
    index_part                         indices[MAX_INDEXPARTS];
    ts_histo                           hist;
    std::size_t                        payload_size;// size of one packet payload (ph->payload_length
    std::size_t                        heap_size;   // size of a heap in bytes
    std::size_t                        heap_count;  // number of heaps inside one group
    std::size_t                        group_first = 0;  // serial number of the first group (needed for index calculation)
    std::size_t                        group_step = 0;   // the serial number difference between consecutive groups
    std::size_t                        cts_data;
    std::size_t                        cts_temp;
    std::size_t                        nsci;
    std::vector<std::size_t>           scis;
    int                                state = INIT_STATE;
    bool                               hasStarted = false;
    statistics                         tstat;
    et_statistics                      et;
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
    void show_mark_log();
    void show_state_log();
  };
  
}

#endif /* mkrecv_allocator_h */
