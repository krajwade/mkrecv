#ifndef mkrecv_rnt_h
#define mkrecv_rnt_h

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <thread>

#include <boost/program_options.hpp>
#include <boost/asio.hpp>

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

#ifndef USE_STD_MUTEX
#include "spead2/common_semaphore.h"
#endif

#include "ipcbuf.h"
#include "dada_hdu.h"
#include "dada_def.h"
#include "multilog.h"

#include "mkrecv_common.h"

//#define COMBINED_IP_PORT

#define DADA_TIMESTR "%Y-%m-%d-%H:%M:%S"
#define SAMPLE_CLOCK_START_KEY  "SAMPLE_CLOCK_START"
#define UTC_START_KEY           "UTC_START"


/* Configuration file option */
#define HEADER_OPT         "header"

/* Flags, therefore all default values are false */
#define QUIET_OPT          "quiet"

#define DESCRIPTORS_OPT    "descriptors"

#define JOINT_OPT          "joint"

/* the following options should have sufficient default values */
#define PACKET_NBYTES_OPT         "packet-size"
#define PACKET_NBYTES_KEY         "PACKET_SIZE"

#define BUFFER_NBYTES_OPT         "buffer-size"
#define BUFFER_NBYTES_KEY         "BUFFER_SIZE"

#define NTHREADS_OPT       "nthreads"
#define NTHREADS_KEY       "NTHREADS"

#define NHEAPS_OPT         "nheaps"
#define NHEAPS_KEY         "NHEAPS"

/* The following options describe the DADA ringbuffer use */

#define DADA_KEY_OPT       "dada-key"
#define DADA_KEY_KEY       "DADA_KEY"

#define DADA_NSLOTS_OPT    "dada-nslots"
#define DADA_NSLOTS_KEY    "DADA_NSLOTS"

/* The following options describe the connection to the network */

#if SPEAD2_USE_IBV
#define IBV_IF_OPT         "ibv-if"
#define IBV_IF_KEY         "IBV_IF"

#define IBV_VECTOR_OPT     "ibv-vector"
#define IBV_VECTOR_KEY     "IBV_VECTOR"

#define IBV_MAX_POLL_OPT   "ibv-max-poll"
#define IBV_MAX_POLL_KEY   "IBV_MAX_POLL"
#endif

#define UDP_IF_OPT         "udp-if"
#define UDP_IF_KEY         "UDP_IF"

#define PORT_OPT           "port"
#define PORT_KEY           "PORT"

#define SOURCES_OPT        "source"
#define SOURCES_KEY        "MCAST_SOURCES"

/* The following options describe the timing information */
#define SYNC_EPOCH_OPT     "sync-epoch"
#define SYNC_EPOCH_KEY     "SYNC_TIME"

#define SAMPLE_CLOCK_OPT   "sample-clock"
#define SAMPLE_CLOCK_KEY   "SAMPLE_CLOCK"

/* The following options describe the item -> index mapping */

#define NINDICES_OPT       "nindices"
#define NINDICES_KEY       "NINDICES"

#define IDX_ITEM_OPT       "idx%ld-item"
#define IDX_ITEM_KEY       "IDX%ld_ITEM"
#define IDX_ITEM_DESC      "Item pointer index for index component %ld"

#define IDX_STEP_OPT       "idx1-step"
#define IDX_STEP_KEY       "IDX1_STEP"
#define IDX_STEP_DESC      "The difference between successive timestamps"

#define IDX_MOD_OPT        "idx1-modulo"
#define IDX_MOD_KEY        "IDX1_MODULO"
#define IDX_MOD_DESC       "Sets the first timestamp to a multiple of this value"

#define IDX_MASK_OPT       "idx%ld-mask"
#define IDX_MASK_KEY       "IDX%ld_MASK"
#define IDX_MASK_DESC      "Mask for using only a part of an item value for index component %ld"

#define IDX_LIST_OPT       "idx%ld-list"
#define IDX_LIST_KEY       "IDX%ld_LIST"
#define IDX_LIST_DESC      "A List of item pointer values for index component %ld"

/* It is possible to specify the heap size and use it as a filter, otherwise the first heap is used to determine this size. */
#define HEAP_NBYTES_OPT      "heap-size"
#define HEAP_NBYTES_KEY      "HEAP_NBYTES"

#define LEVEL_DATA_OPT     "level-data"
#define LEVEL_DATA_KEY     "LEVEL_DATA"

/* It is possible to put a list of item pointer values into some kind of side-channel inside a ringbuffer slot */
#define SCI_LIST_OPT       "sci-list"
#define SCI_LIST_KEY       "SCI_LIST"

namespace po = boost::program_options;
namespace asio = boost::asio;

namespace mkrecv
{

  static constexpr std::int64_t  MAX_INDICES            =      8;
  static constexpr std::int64_t  MAX_SIDE_CHANNEL_ITEMS =     16;
  static constexpr std::int64_t  MAX_OPEN_HEAPS         =    256;
  static constexpr std::int64_t  LOG_FREQ               = 100000;
  static constexpr char          ASCII_HEADER_SENTINEL  =      4;

  typedef enum {DEFAULT_USED, CONFIG_USED, OPTION_USED} USED_TYPE;

  // *******************************************************************
  // ***** class storage_statistics
  // *******************************************************************
  class storage_statistics
  {
  public:
    std::int64_t    heaps_total     = 0;  // number of received heaps
    std::int64_t    heaps_completed = 0;  // number of completed heaps
    std::int64_t    heaps_discarded = 0;  // number of discarded heaps
    std::int64_t    heaps_skipped   = 0;  // number of skipped heaps due to timestamp value
    std::int64_t    heaps_overrun   = 0;  // number of lost heaps due to overrun
    std::int64_t    heaps_ignored   = 0;  // number of ignored heaps (requested destination is TRASH)
    std::int64_t    heaps_too_old   = 0;  // number of heaps were the timestamp is too old
    std::int64_t    heaps_present   = 0;  // number of heaps with the correct timestamp
    std::int64_t    heaps_too_new   = 0;  // number of heaps were the timestamp is too new
    std::int64_t    heaps_open      = 0;  // number of open heaps
    std::int64_t    heaps_needed    = 0;  // number of needed heaps
    std::int64_t    bytes_expected  = 0;  // number of expected payload bytes
    std::int64_t    bytes_received  = 0;  // number of received payload bytes
  public:
    storage_statistics();
    void reset();
  };

#ifdef ENABLE_TIMING_MEASUREMENTS
  // *******************************************************************
  // ***** class timing_statistics
  // *******************************************************************
  class timing_statistics
  { // timing histogram with logarithmic slots, sequence 1 - 2 - 5, starting with 10 ns up to 1 s
  public:
    static constexpr std::int64_t ALLOC_TIMING = 0;
    static constexpr std::int64_t MARK_TIMING  = 1;
    static constexpr std::int64_t MAX_SLOTS    = 8*3+1;
    static constexpr std::int64_t LARGER_SLOT  = MAX_SLOTS-1;
  public:
    double   min_et[2];
    double   max_et[2];
    double   sum_et[2];
    double   count_et[2];
    std::int64_t     histo[2][MAX_SLOTS]; // 8 times 1,2,5
  public:
    timing_statistics();
    void add_et(std::int64_t fi, double nanoseconds);
    void show();
    void reset();
  };
#endif

  // *******************************************************************
  // ***** class index
  // *******************************************************************
  class index
  {
  public:
    // options from header file or command line parameter
    std::string                item_str       = "0";
    USED_TYPE                  item_used_type = DEFAULT_USED;
    std::int64_t               item           =  0;  // IDXi_ITEM
    std::string                mask_str       = "0xffffffffffff";
    USED_TYPE                  mask_used_type = DEFAULT_USED;
    std::int64_t               mask           = 0xffffffffffff;  // IDXi_MASK
    std::string                step_str       = "0";
    USED_TYPE                  step_used_type = DEFAULT_USED;
    std::int64_t               step           =  0;  // IDXi_STEP
    std::string                mod_str        = "0";
    USED_TYPE                  mod_used_type  = DEFAULT_USED;
    std::int64_t               mod            =  0;  // IDXi_MODULO
    std::string                list           = "";  // IDXi_LIST
    USED_TYPE                  list_used_type = DEFAULT_USED;
    std::vector<spead2::s_item_pointer_t> values;
    // mapping value to index
    std::unordered_map<spead2::s_item_pointer_t, std::int64_t> value2index;
    spead2::s_item_pointer_t   first    = 0;
    std::int64_t               count    = 0;
    std::int64_t               nerror   = 0;
    std::int64_t               nskipped = 0;
    spead2::s_item_pointer_t   valbits  = 0;
    std::int64_t               valshift = 0;
    spead2::s_item_pointer_t   valmin   = 0;
    spead2::s_item_pointer_t   valmax   = 0;
    spead2::s_item_pointer_t   mapmax   = 0;
    std::int64_t              *map      = NULL;
  public:
    index();
    ~index();
    void update_mapping(std::int64_t index_size);
    std::int64_t v2i(spead2::s_item_pointer_t v, bool uhm = false);
  };

  // *******************************************************************
  // ***** class options
  // *******************************************************************
  class options
  {
  protected:
    bool                      ready = false;
  public:
    // optional header file contain configuration options and additional information
    std::string               hdrname             = "";
    // some flags
    bool                      quiet               = false;
    bool                      descriptors         = false;
    bool                      joint               = false;
    // some options, default values should be ok to use, will _not_ go into header
    std::string               nthreads_str        = "1";
    std::int64_t              nthreads            =  1;  // number of threads in the thread pool
    std::string               nheaps_str          = "64";
    std::int64_t              nheaps              =  64; // number of in-flight open heaps
    bool                      memcpy_nt           = false;
    // DADA ringbuffer related stuff
    std::string               dada_key            = "dada";
    std::string               dada_nslots_str     = "2";
    std::int64_t              dada_nslots         =  2;
    // network configuration
#if SPEAD2_USE_IBV
    std::string               ibv_if              = "";
    std::string               ibv_comp_vector_str = "0";
    std::int64_t              ibv_comp_vector     =  0;
    std::string               ibv_max_poll_str    = "10";
    std::int64_t              ibv_max_poll        =  10;
#endif
    std::string               udp_if              = "";
    std::string               port                = "";
    std::string               sources_str         = "";
    std::vector<std::string>  sources_opt;
    std::vector<std::string>  sources;
    std::string               sync_epoch_str      = "0";
    double                    sync_epoch          =  0;
    std::string               sample_clock_str    = "1750000000.0";
    double                    sample_clock        =  1750000000.0;
    // Index definitions for mapping a heap to an index
    std::string               nindices_str        = "0";
    std::int64_t              nindices            =  0;
    index                     indices[MAX_INDICES];
    std::string               level_data_str      = "50";
    std::int64_t              level_data          =  50;
    std::string               sci_list            = "";
    std::int64_t              nsci                = 0;
    std::vector<std::int64_t> scis;
    // header
    char                     *header              = NULL;
    // sizes
    std::string               packet_nbytes_str   = "9200";
    std::int64_t              packet_nbytes       =  9200;
    std::string               buffer_nbytes_str   = "8388608";
    std::int64_t              buffer_nbytes       =  8388608;
    std::string               heap_nbytes_str     = "0";
    std::int64_t              heap_nbytes         =  0;  // size of a heap in bytes;
    std::int64_t              group_nbytes        =  0;  // size of a heap group in bytes
    std::int64_t              group_nheaps        =  0;  // number of heaps in a heap group
  protected:
    po::options_description              desc;
    po::options_description              hidden;
    po::options_description              all;
    po::positional_options_description   positional;
    po::variables_map                    vm;
  public:
    options();
    virtual ~options();
    void usage(std::ostream &o);
    void parse_args(int argc, const char **argv);
    void load_header();
    void set_start_time(int64_t timestamp);
    bool check_header();
    virtual void create_args();
    virtual void apply_header();
    std::int64_t p2i(spead2::recv::packet_header *ph, std::size_t size, spead2::s_item_pointer_t &timestamp);
  protected:
    USED_TYPE finalize_parameter(std::string &val_str, const char *opt, const char *key);
    bool parse_fixnum(std::int64_t &val, std::string &val_str);
    USED_TYPE parse_parameter(std::string &val, const char *opt, const char *key);
    USED_TYPE parse_parameter(std::int64_t &val, std::string &val_str, const char *opt, const char *key);
    USED_TYPE parse_parameter(double &val, std::string &val_str, const char *opt, const char *key);
    USED_TYPE parse_parameter(bool &val, std::string &val_str, const char *opt, const char *key);
    USED_TYPE parse_parameter(std::vector<std::int64_t> &val, std::string &val_str, const char *opt, const char *key);
    USED_TYPE parse_parameter(std::vector<std::string> &val, std::string &val_str, const char *opt, const char *key);
    bool check_index_specification();
  };

  // *******************************************************************
  // ***** class storage
  // *******************************************************************
  /*
    This class manages a given number of open dada ringbuffer slots and an additional buffer as TRASH (large enough for one heap group).
    The open slots form a sequential memory where the first active slot (indexed by buffer_first) starts with the heap group with the
    timestamp timestamp_first. If a heap is finished which timestamp is beyond timestamp_level, this first active slot is released,
    a new one allocated (replaces the buffer at buffer_first) and buffer_first is incremented modulo (nbuffers). This means no active
    pointers in buffers[], payload_base[], and sci_base[] will be changed except the entries indexed with buffer_first. This ensures, that
    the expected payload and the received payload, ... goes to the correct statistics.
    The first nbuffers buffers (index: [0 .. nbuffers-1]) reflect dada ringbuffer slots, whereas the last buffer (index: [nbuffers])
    is used as trash which is large enough to contain exactly one heap group. This allows that the calling class can still add
    the heap index inside a heap group to the base address returned by calling alloc_place().
  */
  class storage
  {
  protected:
#ifdef USE_STD_MUTEX
    std::mutex                         dest_mutex;
#else
    spead2::semaphore_spin             dest_sem;
#endif
    std::shared_ptr<mkrecv::options>          opts;
    std::shared_ptr<spead2::mmap_allocator>   memallocator;
    // connection to DADA
    key_t                      key;
    multilog_t                *mlog = NULL;
    dada_hdu_t                *dada = NULL;
    // sizes
    std::int64_t               heap_nbytes;   // size of a heap in bytes;
    std::int64_t               group_nbytes;  // size of a heap group in bytes
    std::int64_t               group_nheaps;  // number of heaps in a heap group
    std::int64_t               slot_nbytes;   // slot size in bytes
    std::int64_t               slot_nheaps;   // number of heaps which can go into a slot
    std::int64_t               slot_ngroups;  // number of heap groups which can go into a slot
    std::int64_t               nslots;        // total number of slots in the DADA ringbuffer
    // heap/heap group information:
    std::int64_t               nsci;          // number of items in the side channel per heap
    // buffer[0 .. nbuffers-1] := DATA (dada ringbuffer slots)
    // buffer[nbuffers ]       := TRASH
    std::int64_t               nbuffers         = 0;    // number of available buffers, except trash
    std::int64_t               buffer_first     = 0;    // index of the first active buffer inside the buffer ring, starts with timestamp_first
    std::int64_t               buffer_active    = 0;    // number of active buffers, may change during slot switching
    std::uint64_t             *indices          = NULL; // indices of all open slots (nbuffers+1 entries)
    char                     **buffers          = NULL; // available buffers (nbuffers+1 entries)
    char                     **payload_base     = NULL; // address of the beginning of the payload in a buffer (nbuffers+1 entries)
    spead2::s_item_pointer_t **sci_base         = NULL; // address of the beginning of the side channel items in a buffer (nbuffers+1 entries)
    std::int64_t               trash_nbytes     = 0;    // trash size in bytes (payload and side channel items)
    char                      *trash_base       = NULL; // trash memory base
    storage_statistics        *bstat            = NULL; // statistics for each buffer (nbuffers+1 entries)
    storage_statistics         gstat;                   // global statistics (total numbers)
#ifdef ENABLE_TIMING_MEASUREMENTS
    timing_statistics          et;
#endif
    // timestamps needed to calculate the data buffer index:
    // 
    spead2::s_item_pointer_t   timestamp_first  = 0;    // serial number of the first group (needed for index calculation)
    spead2::s_item_pointer_t   timestamp_step   = 0;    // the serial number difference between consecutive groups
    spead2::s_item_pointer_t   timestamp_mod    = 0;    // first timestamp is a multiple of this value
    spead2::s_item_pointer_t   timestamp_offset = 0;    // number of timestamps after timestamp_first when a buffer_first is released and a new one is allocated
    spead2::s_item_pointer_t   timestamp_level  = 0;    // timestamp_first + timestamp_step*timestamp_offset
    // handle stopping
    bool                               stop = false;
    bool                               has_stopped = false;
    std::int64_t                        log_counter = 0;
    // needed to fill the ringbuffer header once
    std::mutex                         header_mutex;
    std::condition_variable            header_cv;
    std::thread                        header_thread;
    // needed to release the first slot (lowest timestamps), allocate a new slot (highest timestamps) and update the internal data structures
    std::mutex                         switch_mutex;
    std::condition_variable            switch_cv;
    std::thread                        switch_thread;
    bool                               switch_triggered = false;
  public:
    storage(std::shared_ptr<mkrecv::options> opts);
    ~storage();
    // The alloc_data() method returns the base address to a heap group. Any further calculation
    // (address for a heap inside a heap group) must be done inside the calling class. This should
    // save time in a guarded pice of code.
    //   IN     timestamp  := SPEAD timestamp from heap
    //   IN     explen     := expected number of bytes (payload size)
    //   OUT    hg_payload := start address for heaps of a heap group defined by timestamp
    //   OUT    hg_sci     := start address for side channel items of a heap group defined by timestamp
    //   RETURN dest_index := (real) destination index: [0 .. nbuffers-2] -> DATA, nbuffers-1 -> TRASH
    std::int64_t alloc_data(spead2::s_item_pointer_t timestamp,
		   std::int64_t explen,
		   char *&hg_payload,
		   spead2::s_item_pointer_t *&hg_sci);
    // The alloc_trash() method returns the base address to a heap group inside the trash.
    // Any further calculation (address for a heap inside a heap group) must be done inside
    // the calling class.
    //   IN     timestamp  := SPEAD timestamp from heap
    //   IN     explen     := expected number of bytes (payload size)
    //   OUT    hg_payload := start address for heaps of a heap group defined by timestamp
    //   OUT    hg_sci     := start address for side channel items of a heap group defined by timestamp
    //   RETURN dest_index := (real) destination index: [0 .. nbuffers-2] -> DATA, nbuffers-1 -> TRASH
    std::int64_t alloc_trash(spead2::s_item_pointer_t timestamp,
		    std::int64_t explen,
		    char *&hg_payload,
		    spead2::s_item_pointer_t *&hg_sci);
    // The free_place() method finalize the recieving of a heap and reports the recieved number of bytes (may be less than explen).
    //   IN timestamp  := SPEAD timestamp from heap
    //   IN dest_index := destination index, from previous alloc_place() call
    //   IN reclen     := recieved number of bytes
    void free_place(spead2::s_item_pointer_t timestamp, std::int64_t dest_index, std::int64_t reclen);
    inline std::int64_t get_trash_dest_index() { return nbuffers; };
    // Request stopping, filling the latest slot (the last currently open slot!) and closing the dada ringbuffer.
    void request_stop();
    // Test if storage is closed (some time after request_stop() call).
    bool is_stopped();
    // Attaching to the DADA ringbuffer, open a header and allocate a number of open slots
    void open();
    // Close the dada ringbuffer access by releasing an empty slot.
    void close();
  protected:
    void create_header();
    void move_ringbuffer();
  };

  // *******************************************************************
  // ***** class allocator
  // *******************************************************************
  /*
   * This allocator uses three memory areas for putting the incomming heaps into. The main
   * memory area is the current ringbuffer slot provided by the PSR_DADA library. Each slot
   * contains a sequence of heaps. A certain number of heaps are grouped together (normally
   * by a common timestamp value). This 
   */
  class allocator : public spead2::memory_allocator
  {
  protected:
  protected:
    std::shared_ptr<mkrecv::options>   opts = NULL;
    std::shared_ptr<mkrecv::storage>   store;
    std::int64_t                       head = 0;
    std::int64_t                       tail = 0;
    spead2::s_item_pointer_t           heap_id[MAX_OPEN_HEAPS];
    std::int64_t                               heap_dest[MAX_OPEN_HEAPS];
    spead2::s_item_pointer_t           heap_timestamp[MAX_OPEN_HEAPS];
    std::int64_t                       payload_nbytes;// size of one packet payload (ph->payload_length
    // sizes
    std::int64_t                       heap_nbytes;   // size of a heap in bytes;
    std::int64_t                       group_nbytes;  // size of a heap group in bytes
    std::int64_t                       group_nheaps;  // number of heaps in a heap group
    std::int64_t                       timestamp_item = 0;
    std::int64_t                       nsci;
    std::int64_t                       scis[MAX_SIDE_CHANNEL_ITEMS];
    bool                               has_started = false;
    bool                               stop        = false;
    bool                               has_stopped = false;
    std::int64_t                       heaps_total = 0;
#ifdef ENABLE_TIMING_MEASUREMENTS
    timing_statistics                  et;
#endif
  public:
    allocator(std::shared_ptr<mkrecv::options> opts, std::shared_ptr<mkrecv::storage> store);
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

  // *******************************************************************
  // ***** class stream
  // *******************************************************************
  class stream : public spead2::recv::stream
  {
  private:
    std::int64_t                            n_complete = 0;
    const std::shared_ptr<mkrecv::options>  opts = NULL;
    std::promise<void>                      stop_promise;
    std::shared_ptr<mkrecv::allocator>      ha = NULL;
    
    void show_heap(const spead2::recv::heap &fheap);
    virtual void heap_ready(spead2::recv::live_heap &&heap) override;

  public:
    template<typename... Args>
      stream(const std::shared_ptr<options> opts, Args&&... args)
      : spead2::recv::stream::stream(std::forward<Args>(args)...),
      opts(opts) {}
    
    void stop_received(); // override;
    std::int64_t join();
    void set_allocator(std::shared_ptr<mkrecv::allocator> ha);
  };

  // *******************************************************************
  // ***** class receiver
  // *******************************************************************
  class receiver
  {
  protected:
    static receiver     *instance;
  protected:
    std::shared_ptr<mkrecv::options>                   opts = NULL;
    std::shared_ptr<mkrecv::storage>                   storage = NULL;
    std::vector<std::shared_ptr<mkrecv::allocator> >   allocators;
    std::vector<std::unique_ptr<mkrecv::stream> >      streams;
    std::shared_ptr<spead2::thread_pool>       thread_pool = NULL;
  public:
    receiver();
    virtual ~receiver();
    int execute(int argc, const char **argv);
    std::unique_ptr<stream> create_stream();
  public:
    static void request_stop();
  protected:
    std::unique_ptr<stream> make_stream(std::vector<std::string>::iterator first_source,
					std::vector<std::string>::iterator last_source);
  };

}

#endif /* mkrecv_rnt_h */
