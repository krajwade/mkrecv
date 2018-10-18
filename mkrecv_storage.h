#ifndef mkrecv_storage_h
#define mkrecv_storage_h

#include <cstdint>
#include <unordered_map>

#include <spead2/common_defines.h>
#include <spead2/common_logging.h>
#include <spead2/common_endian.h>

#include "mkrecv_options.h"

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
    std::size_t    heaps_open      = 0;  // number of open heaps
    std::size_t    bytes_expected  = 0;  // number of expected payload bytes
    std::size_t    bytes_received  = 0;  // number of received payload bytes
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
    std::shared_ptr<mkrecv::options>   opts;
    std::shared_ptr<spead2::mmap_allocator>   memallocator;
    std::size_t                        heap_size;   // size of a heap in bytes
    std::size_t                        heap_count;  // number of heaps inside one group
    spead2::s_item_pointer_t           timestamp_first = 0;  // serial number of the first group (needed for index calculation)
    spead2::s_item_pointer_t           timestamp_step = 0;   // the serial number difference between consecutive groups
    std::size_t                        cts_data;
    std::size_t                        cts_temp;
    std::size_t                        nsci;
    std::vector<std::size_t>           scis;
    int                                state = INIT_STATE;
    bool                               stop = false;
    bool                               has_stopped = false;
    storage_statistics                 gstat;
    storage_statistics                 dstat[3];
  public:
    storage(std::shared_ptr<mkrecv::options> opts);
    // This method finds the memory location for a heap and his side-channel items.
    // The return value encodes the used destination which is used to mark a heap as received.
    virtual int alloc_place(spead2::s_item_pointer_t timestamp,    // timestamp of a heap
			    std::size_t heap_index,                // heap number inside a heap group
			    std::size_t size,                      // heap size (only payload)
			    int dest_index,                        // requested destination (DATA_DEST _or_ TRASH_DEST)
			    char *&heap_place,                     // returned memory pointer to this heap payload
			    spead2::s_item_pointer_t *&sci_place)  // returned memory pointer to the side-channel items for this heap
      = 0;
    virtual void free_place(int dest,            // real destination of a heap (DATA_DEST, TEMP_DEST or TRASH_DEST)
			    std::size_t reclen)  // recieved number of bytes
      = 0;
    virtual void request_stop() = 0;
    virtual bool is_stopped() = 0;
    virtual void close() = 0;
  };

}

#endif /* mkrecv_storage_h */

