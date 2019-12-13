#ifndef mkrecv_storage_h
#define mkrecv_storage_h

#include <cstdint>
#include <unordered_map>

#include <spead2/common_defines.h>
#include <spead2/common_logging.h>
#include <spead2/common_endian.h>
#ifndef USE_STD_MUTEX
#include "spead2/common_semaphore.h"
#endif

#include "mkrecv_options.h"
#include "mkrecv_allocator.h"

namespace mkrecv
{

  class storage_statistics
  {
  public:
    std::size_t    heaps_total     = 0;  // number of received heaps
    std::size_t    heaps_completed = 0;  // number of completed heaps
    std::size_t    heaps_discarded = 0;  // number of discarded heaps
    std::size_t    heaps_skipped   = 0;  // number of skipped heaps due to timestamp value
    std::size_t    heaps_overrun   = 0;  // number of lost heaps due to overrun
    std::size_t    heaps_ignored   = 0;  // number of ignored heaps (requested destination is TRASH)
    std::size_t    heaps_too_old   = 0;  // number of heaps were the timestamp is too old
    std::size_t    heaps_present   = 0;  // number of heaps with the correct timestamp
    std::size_t    heaps_too_new   = 0;  // number of heaps were the timestamp is too new
    std::size_t    heaps_open      = 0;  // number of open heaps
    std::size_t    heaps_needed    = 0;  // number of needed heaps
    std::size_t    bytes_expected  = 0;  // number of expected payload bytes
    std::size_t    bytes_received  = 0;  // number of received payload bytes
  protected:
    bool           reset_flag = false;
  public:
    storage_statistics();
    void reset();
  };

  class storage
  {
  public:
    // Some default sizes
    static const std::size_t  MAX_DATA_SPACE = 256*1024*1024;
    static const std::size_t  MAX_TEMPORARY_SPACE = 64*4096*128;
    // Enumeration for different types of destinations
    static const int DATA_DEST  = 0;
    static const int TEMP_DEST  = 1;
    static const int TRASH_DEST = 2;
    // Enumeration for internal state machine
    static const int INIT_STATE       = 0;
    static const int SEQUENTIAL_STATE = 1;
    static const int PARALLEL_STATE   = 2;
    // Some additional constants
    static const int LOG_FREQ = 100000;
  protected:
#ifdef USE_STD_MUTEX
    std::mutex                         dest_mutex;
#else
    spead2::semaphore_spin             dest_sem;
#endif
    std::shared_ptr<mkrecv::options>   opts;
    std::shared_ptr<spead2::mmap_allocator>   memallocator;
    destination                        dest[3];
    std::size_t                        heap_size;   // size of a heap in bytes
    std::size_t                        heap_count;  // number of heaps inside one group
    spead2::s_item_pointer_t           timestamp_first = 0;  // serial number of the first group (needed for index calculation)
    spead2::s_item_pointer_t           timestamp_step = 0;   // the serial number difference between consecutive groups
    spead2::s_item_pointer_t           timestamp_mod  = 0;   // first timestamp is a multiple of this value
    spead2::s_item_pointer_t           timestamp_data_count = 0;
    spead2::s_item_pointer_t           timestamp_temp_count = 0;
    spead2::s_item_pointer_t           timestamp_data_level = 0; // timestamp for switching to sequential mode
    spead2::s_item_pointer_t           timestamp_temp_level = 0; // timestamp for switching to parallel mode
    std::size_t                        nsci;
    std::vector<std::size_t>           scis;
    int                                state = INIT_STATE;
    bool                               stop = false;
    bool                               has_stopped = false;
    std::size_t                        log_counter = 0;
    storage_statistics                 gstat;
    storage_statistics                 dstat[3];
#ifdef ENABLE_TIMING_MEASUREMENTS
    et_statistics                      et;
#endif
  public:
    storage(std::shared_ptr<mkrecv::options> opts);
    ~storage();
    // This method finds the memory location for a heap and his side-channel items.
    // The return value encodes the used destination which is used to mark a heap as received.
    int alloc_place(spead2::s_item_pointer_t timestamp,    // timestamp of a heap
		    std::size_t heap_index,                // heap number inside a heap group
		    std::size_t size,                      // heap size (only payload)
		    int dest_index,                        // requested destination (DATA_DEST _or_ TRASH_DEST)
		    char *&heap_place,                     // returned memory pointer to this heap payload
		    spead2::s_item_pointer_t *&sci_place); // returned memory pointer to the side-channel items for this heap
    void free_place(spead2::s_item_pointer_t timestamp,    // timestamp of a heap
		    int dest_index,                        // real destination of a heap (DATA_DEST, TEMP_DEST or TRASH_DEST)
		    std::size_t reclen);                   // recieved number of bytes
    virtual void request_stop() = 0;
    virtual bool is_stopped() = 0;
    virtual void close() = 0;
  protected:
    virtual void handle_init();
    virtual int handle_alloc_place(spead2::s_item_pointer_t &group_index, int dest_index);
    virtual void handle_free_place(spead2::s_item_pointer_t timestamp, int dest_index);
    void show_log();
  };

}

#endif /* mkrecv_storage_h */

