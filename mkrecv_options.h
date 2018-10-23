#ifndef mkrecv_options_h
#define mkrecv_options_h

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <boost/program_options.hpp>

#include <spead2/recv_udp.h>
#if SPEAD2_USE_IBV
# include <spead2/recv_udp_ibv.h>
#endif

#include "dada_def.h"

#define MAX_INDEXPARTS 4

#define DADA_TIMESTR "%Y-%m-%d-%H:%M:%S"
#define SAMPLE_CLOCK_START_KEY  "SAMPLE_CLOCK_START"
#define UTC_START_KEY           "UTC_START"


/* Configuration file option */
#define HEADER_OPT         "header"
#define HEADER_DESC        "name of a template header file which may include configuration options"
#define HEADER_DEF         ""

/* Flags, therefore all default values are false */
#define QUIET_OPT          "quiet"
#define QUIET_KEY          ""
#define QUIET_DESC         "Only show total of heaps received"

#define DESCRIPTORS_OPT    "descriptors"
#define DESCRIPTORS_KEY    ""
#define DESCRIPTORS_DESC   "Show descriptors"

#define PYSPEAD_OPT        "pyspead"
#define PYSPEAD_KEY        ""
#define PYSPEAD_DESC       "Be bug-compatible with PySPEAD"

#define JOINT_OPT          "joint"
#define JOINT_KEY          ""
#define JOINT_DESC         "Treat all sources as a single stream"

/* the following options should have sufficient default values */
#define PACKET_OPT         "packet-size"
#define PACKET_KEY         "PACKET_SIZE"
#define PACKET_DESC        "Maximum packet size to use for UDP"
#define PACKET_DEF         spead2::recv::udp_reader::default_max_size

#define BUFFER_OPT         "buffer-size"
#define BUFFER_KEY         "BUFFER_SIZE"
#define BUFFER_DESC        "Socket buffer size"
#define BUFFER_DEF         spead2::recv::udp_reader::default_buffer_size

#define NTHREADS_OPT       "nthreads"
#define NTHREADS_KEY       "NTHREADS"
#define NTHREADS_DESC      "Number of worker threads"
#define NTHREADS_DEF       1

#define NHEAPS_OPT         "nheaps"
#define NHEAPS_KEY         "NHEAPS"
#define NHEAPS_DESC        "Maximum number of in-flight heaps"
#define NHEAPS_DEF         spead2::recv::stream::default_max_heaps

/* The following options describe the DADA ringbuffer use */

#define DADA_MODE_OPT      "dada-mode"
#define DADA_MODE_KEY      "DADA_MODE"
#define DADA_MODE_DESC     "dada mode (0 = no, 1 = huge trash, 2 = dada, 3 = dada+slot, 4 = full dada)"
#define NO_DADA_MODE       0
#define TRASH_DADA_MODE    1
#define STATIC_DADA_MODE   2
#define DYNAMIC_DADA_MODE  3
#define FULL_DADA_MODE     4
#define DADA_MODE_DEF      FULL_DADA_MODE

#define DADA_KEY_OPT       "dada-key"
#define DADA_KEY_KEY       "DADA_KEY"
#define DADA_KEY_DESC      "PSRDADA ring buffer key"
#define DADA_KEY_DEF       "dada"

/* The following options describe the connection to the network */

#if SPEAD2_USE_IBV
#define IBV_IF_OPT         "ibv-if"
#define IBV_IF_KEY         "IBV_IF"
#define IBV_IF_DESC        "Interface address for ibverbs"
#define IBV_IF_DEF         ""

#define IBV_VECTOR_OPT     "ibv-vector"
#define IBV_VECTOR_KEY     "IBV_VECTOR"
#define IBV_VECTOR_DESC    "Interrupt vector (-1 for polled)"
#define IBV_VECTOR_DEF     0

#define IBV_MAX_POLL_OPT   "ibv-max-poll"
#define IBV_MAX_POLL_KEY   "IBV_MAX_POLL"
#define IBV_MAX_POLL_DESC  "Maximum number of times to poll in a row"
#define IBV_MAX_POLL_DEF   spead2::recv::udp_ibv_reader::default_max_poll
#endif

#define UDP_IF_OPT         "udp-if"
#define UDP_IF_KEY         "UDP_IF"
#define UDP_IF_DESC        "UDP interface"
#define UDP_IF_DEF         ""

#define PORT_OPT           "port"
#define PORT_KEY           "PORT"
#define PORT_DESC          "Port number"
#define PORT_DEF           ""

#define SOURCES_OPT        "source"
#define SOURCES_KEY        "MCAST_SOURCES"
#define SOURCES_DESC       "sources"

/* The following options describe the timing information */
#define SYNC_EPOCH_OPT     "sync-epoch"
#define SYNC_EPOCH_KEY     "SYNC_TIME"
#define SYNC_EPOCH_DESC    "the ADC sync epoch"
#define SYNC_EPOCH_DEF     0.0

#define SAMPLE_CLOCK_OPT   "sample-clock"
#define SAMPLE_CLOCK_KEY   "SAMPLE_CLOCK"
#define SAMPLE_CLOCK_DESC  "virtual sample clock used for calculations"
#define SAMPLE_CLOCK_DEF   1750000000.0

/* The following options describe the item -> index mapping */

#define NINDICES_OPT       "nindices"
#define NINDICES_KEY       "NINDICES"
#define NINDICES_DESC      "Number of item pointers used as index"
#define NINDICES_DEF       0

#define IDX_ITEM_OPT      "idx%d-item"
#define IDX_ITEM_KEY      "IDX%d_ITEM"
#define IDX_ITEM_DESC     "Item pointer index for index component %d"
#define IDX_ITEM_DEF      0

#define IDX_STEP_OPT      "idx%d-step"
#define IDX_STEP_KEY      "IDX%d_STEP"
#define IDX_STEP_DESC     "The difference between successive item pointer values for index component %d"
#define IDX_STEP_DEF      0x200000   // 2^21

#define IDX_MASK_OPT      "idx%d-mask"
#define IDX_MASK_KEY      "IDX%d_MASK"
#define IDX_MASK_DESC     "Mask for using only a part of an item value for index component %d"
#define IDX_MASK_DEF      0xffffffffffff

#define IDX_LIST_OPT      "idx%d-list"
#define IDX_LIST_KEY      "IDX%d_LIST"
#define IDX_LIST_DESC     "A List of item pointer values for index component %d"
#define IDX_LIST_DEF      ""

/* It is possible to specify the heap size and use it as a filter, otherwise the first heap is used to determine this size. */
#define HEAP_SIZE_OPT      "heap-size"
#define HEAP_SIZE_KEY      "HEAP_SIZE"
#define HEAP_SIZE_DESC     "The heap size used for checking incomming heaps."
#define HEAP_SIZE_DEF      0

#define NGROUPS_DATA_OPT   "ngroups-data"
#define NGROUPS_DATA_KEY   "NGROUPS_DATA"
#define NGROUPS_DATA_DESC  "Number of groups (heaps with the same timestamp) going into the data space."
#define NGROUPS_DATA_DEF   64

#define NGROUPS_TEMP_OPT   "ngroups-temp"
#define NGROUPS_TEMP_KEY   "NGROUPS_TEMP"
#define NGROUPS_TEMP_DESC  "Number of groups (heaps with the same timestamp) going into the temporary space."
#define NGROUPS_TEMP_DEF   32

#define LEVEL_DATA_OPT     "level-data"
#define LEVEL_DATA_KEY     "LEVEL_DATA"
#define LEVEL_DATA_DESC    "Fill level of the data destination before switching to sequential mode (percent)."
#define LEVEL_DATA_DEF     50

#define LEVEL_TEMP_OPT     "level-temp"
#define LEVEL_TEMP_KEY     "LEVEL_TEMP"
#define LEVEL_TEMP_DESC    "Fill level of the temporary destination before switching to parallel mode (percent)."
#define LEVEL_TEMP_DEF     50

#define NHEAPS_SWITCH_OPT   "nheaps-switch"
#define NHEAPS_SWITCH_KEY   "NHEAPS_SWITCH"
#define NHEAPS_SWITCH_DESC  "Number of received heaps before switching to a new ringbuffer slot."
#define NHEAPS_SWITCH_DEF   0

/* It is possible to put a list of item pointer values into some kind of side-channel inside a ringbuffer slot */
#define SCI_LIST_OPT       "sci-list"
#define SCI_LIST_KEY       "SCI_LIST"
#define SCI_LIST_DESC      "list of item pointers going into a side-channel inside a ringbuffer slot"
#define SCI_LIST_DEF       ""


namespace po = boost::program_options;

namespace mkrecv
{

  static const char  ASCII_HEADER_SENTINEL = 4;

  class index_options
  {
  public:
    std::size_t                           item   = IDX_ITEM_DEF;  // IDXi_ITEM
    std::size_t                           mask   = IDX_MASK_DEF;  // IDXi_MASK
    std::size_t                           step   = IDX_STEP_DEF;  // IDXi_STEP
    std::string                           list   = IDX_LIST_DEF;  // IDXi_LIST
    std::vector<spead2::s_item_pointer_t> values;
  };

  class options
  {
  protected:
    bool                      ready = false;
  public:
    // optional header file contain configuration options and additional information
    std::string               hdrname         = HEADER_DEF;
    // some flags
    bool                      quiet           = false;
    bool                      descriptors     = false;
    bool                      pyspead         = false;
    bool                      joint           = false;
    // some options, default values should be ok to use, will _not_ go into header
    std::size_t               packet          = PACKET_DEF;
    std::size_t               buffer          = BUFFER_DEF;
    int                       threads         = NTHREADS_DEF;
    std::size_t               heaps           = NHEAPS_DEF;
    bool                      memcpy_nt       = false;
    // DADA ringbuffer related stuff
    std::size_t               dada_mode       = DADA_MODE_DEF;
    std::string               dada_key        = DADA_KEY_DEF;
    // network configuration
#if SPEAD2_USE_IBV
    std::string               ibv_if          = IBV_IF_DEF;
    int                       ibv_comp_vector = IBV_VECTOR_DEF;
    int                       ibv_max_poll    = IBV_MAX_POLL_DEF;
#endif
    std::string               udp_if          = UDP_IF_DEF;
    std::string               port            = PORT_DEF;
    std::vector<std::string>  sources;
    std::string               used_sources;
    double                    sync_epoch      = SYNC_EPOCH_DEF;
    double                    sample_clock    = SAMPLE_CLOCK_DEF;
    // Index definitions for mapping a heap to an index
    int                       nindices        = 0;
    index_options             indices[MAX_INDEXPARTS];
    std::size_t               heap_size       = HEAP_SIZE_DEF;
    std::size_t               ngroups_data    = NGROUPS_DATA_DEF;
    std::size_t               ngroups_temp    = NGROUPS_TEMP_DEF;
    std::size_t               level_data      = LEVEL_DATA_DEF;
    std::size_t               level_temp      = LEVEL_TEMP_DEF;
    std::size_t               nheaps_switch   = NHEAPS_SWITCH_DEF;
    std::string               sci_list        = SCI_LIST_DEF;
    std::size_t               nsci            = 0;
    std::vector<std::size_t>  scis;
    // heap filter mechanism
    char                     *header          = NULL;
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
    void set_opt(int &val, const char *opt, const char *key);
    void set_opt(std::size_t &val, const char *opt, const char *key);
    void set_opt(std::string &val, const char *opt, const char *key);
    void set_opt(bool &val, const char *opt, const char *key);
    void set_opt(double &val, const char *opt, const char *key);
    void set_opt(std::vector<std::size_t> &val, const char *opt, const char *key);
    void set_start_time(int64_t timestamp);
    void use_sources(std::vector<std::string> &val, const char *opt, const char *key);
    void update_sources();
    void extract_values(std::vector<spead2::s_item_pointer_t> &val, const std::string &str);
    void extract_values(std::vector<std::size_t> &val, const std::string &str);
    bool check_header();
    virtual void create_args();
    virtual void apply_header();
  };

}

#endif /* mkrecv_options_h */
