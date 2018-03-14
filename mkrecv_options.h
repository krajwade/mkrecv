#ifndef mkrecv_options_h
#define mkrecv_options_h

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include <boost/program_options.hpp>

#include <spead2/recv_udp.h>
#if SPEAD2_USE_NETMAP
# include <spead2/recv_netmap.h>
#endif
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
#define PACKET_OPT         "packet"
#define PACKET_KEY         ""
#define PACKET_DESC        "Maximum packet size to use for UDP"
#define PACKET_DEF         spead2::recv::udp_reader::default_max_size

#define BUFFER_OPT         "buffer"
#define BUFFER_KEY         ""
#define BUFFER_DESC        "Socket buffer size"
#define BUFFER_DEF         spead2::recv::udp_reader::default_buffer_size

#define NTHREADS_OPT       "threads"
#define NTHREADS_KEY       ""
#define NTHREADS_DESC      "Number of worker threads"
#define NTHREADS_DEF       1

#define NHEAPS_OPT         "heaps"
#define NHEAPS_KEY         ""
#define NHEAPS_DESC        "Maximum number of in-flight heaps"
#define NHEAPS_DEF         spead2::recv::stream::default_max_heaps

/* The following options describe the DADA ringbuffer use */

#define DADA_KEY_OPT       "key"
#define DADA_KEY_KEY       "DADA_KEY"
#define DADA_KEY_DESC      "PSRDADA ring buffer key"
#define DADA_KEY_DEF       "dada"

#define DADA_MODE_OPT      "dada"
#define DADA_MODE_KEY      "DADA_MODE"
#define DADA_MODE_DESC     "dada mode (0 = no, 1 = huge trash, 2 = dada, 3 = dada+slot, 4 = full dada"
#define DADA_MODE_DEF      4

/* The following options describe the connection to the F-Engines (network) */

#define NETMAP_IF_OPT      "netmap"
#define NETMAP_IF_KEY      "NETMAP_IF"
#define NETMAP_IF_DESC     "Netmap interface"
#define NETMAP_IF_DEF      ""

#define IBV_IF_OPT         "ibv"
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

#define UDP_IF_OPT         "udp_if"
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
#define SYNC_EPOCH_OPT     "sync_epoch"
#define SYNC_EPOCH_KEY     "SYNC_TIME"
#define SYNC_EPOCH_DESC    "the ADC sync epoch"
#define SYNC_EPOCH_DEF     0.0

#define SAMPLE_CLOCK_OPT   "sample_clock"
#define SAMPLE_CLOCK_KEY   "SAMPLE_CLOCK"
#define SAMPLE_CLOCK_DESC  "virtual sample clock used for calculations"
#define SAMPLE_CLOCK_DEF   1750000000.0

/* The following options describe the item -> index mapping */

#define NINDICES_OPT       "nindices"
#define NINDICES_KEY       "NINDICES"
#define NINDICES_DESC      "Number of item pointers used as index"
#define NINDICES_DEF       0

/* The first item pointer is a running value, therefore no FIRST or COUNT value needed. */
#define IDX1_ITEM_OPT      "idx1_item"
#define IDX1_ITEM_KEY      "IDX1_ITEM"
#define IDX1_ITEM_DESC     "Item pointer index for the first index"
#define IDX1_ITEM_DEF      0

#define IDX1_STEP_OPT      "idx1_step"
#define IDX1_STEP_KEY      "IDX1_STEP"
#define IDX1_STEP_DESC     "The difference between successive item pointer values"
#define IDX1_STEP_DEF      0x200000   // 2^21

/* The second item pointer used as an index (inside the first index). */
#define IDX2_ITEM_OPT      "idx2_item"
#define IDX2_ITEM_KEY      "IDX2_ITEM"
#define IDX2_ITEM_DESC     "Item pointer index for the second index"
#define IDX2_ITEM_DEF      0
/*
#define IDX2_STEP_OPT      "idx2_step"
#define IDX2_STEP_KEY      "IDX2_STEP"
#define IDX2_STEP_DESC     "The difference between successive item pointer values"
#define IDX2_STEP_DEF      0

#define IDX2_FIRST_OPT     "idx2_first"
#define IDX2_FIRST_KEY     "IDX2_FIRST"
#define IDX2_FIRST_DESC    "The first used item pointer value"
#define IDX2_FIRST_DEF     0

#define IDX2_COUNT_OPT     "idx2_count"
#define IDX2_COUNT_KEY     "IDX2_COUNT"
#define IDX2_COUNT_DESC    "The number of used item pointer values"
#define IDX2_COUNT_DEF     0
*/
#define IDX2_LIST_OPT      "idx2_list"
#define IDX2_LIST_KEY      "IDX2_LIST"
#define IDX2_LIST_DESC     "A List of item pointer values for the second index"
#define IDX2_LIST_DEF      ""

/* The third item pointer used as an index (inside the second index). */
#define IDX3_ITEM_OPT      "idx3_item"
#define IDX3_ITEM_KEY      "IDX3_ITEM"
#define IDX3_ITEM_DESC     "Item pointer index for the third index"
#define IDX3_ITEM_DEF      0
/*
#define IDX3_STEP_OPT      "idx3_step"
#define IDX3_STEP_KEY      "IDX3_STEP"
#define IDX3_STEP_DESC     "The difference between successive item pointer values"
#define IDX3_STEP_DEF      0

#define IDX3_FIRST_OPT     "idx3_first"
#define IDX3_FIRST_KEY     "IDX3_FIRST"
#define IDX3_FIRST_DESC    "The first used item pointer value"
#define IDX3_FIRST_DEF     0

#define IDX3_COUNT_OPT     "idx3_count"
#define IDX3_COUNT_KEY     "IDX3_COUNT"
#define IDX3_COUNT_DESC    "The number of used item pointer values"
#define IDX3_COUNT_DEF     0
*/
#define IDX3_LIST_OPT      "idx3_list"
#define IDX3_LIST_KEY      "IDX3_LIST"
#define IDX3_LIST_DESC     "A List of item pointer values for the third index"
#define IDX3_LIST_DEF      ""

/* The fourth item pointer used as an index (inside the third index). */
#define IDX4_ITEM_OPT      "idx4_item"
#define IDX4_ITEM_KEY      "IDX4_ITEM"
#define IDX4_ITEM_DESC     "Item pointer index for the fourth index"
#define IDX4_ITEM_DEF      0
/*
#define IDX4_STEP_OPT      "idx4_step"
#define IDX4_STEP_KEY      "IDX4_STEP"
#define IDX4_STEP_DESC     "The difference between successive item pointer values"
#define IDX4_STEP_DEF      0

#define IDX4_FIRST_OPT     "idx4_first"
#define IDX4_FIRST_KEY     "IDX4_FIRST"
#define IDX4_FIRST_DESC    "The first used item pointer value"
#define IDX4_FIRST_DEF     0

#define IDX4_COUNT_OPT     "idx4_count"
#define IDX4_COUNT_KEY     "IDX4_COUNT"
#define IDX4_COUNT_DESC    "The number of used item pointer values"
#define IDX4_COUNT_DEF     0
*/
#define IDX4_LIST_OPT      "idx4_list"
#define IDX4_LIST_KEY      "IDX4_LIST"
#define IDX4_LIST_DESC     "A List of item pointer values for the fourth index"
#define IDX4_LIST_DEF      ""

namespace po = boost::program_options;

namespace mkrecv
{

  static const char  ASCII_HEADER_SENTINEL = 4;

  class index_options
  {
  public:
    std::size_t                           item   = 0;  // IDXi_ITEM
    std::size_t                           step   = 1;  // IDXi_STEP
    //std::size_t                           first  = 0;  // IDXi_FIRST
    //std::size_t                           count  = 1;  // IDXi_COUNT
    std::string                           list   = ""; // IDXi_LIST
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
    std::string               key             = DADA_KEY_DEF;
    std::size_t               dada_mode       = DADA_MODE_DEF;
    // network configuration
#if SPEAD2_USE_NETMAP
    std::string               netmap_if       = NETMAP_IF_DEF;
#endif
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
    void set_start_time(int64_t timestamp);
    void use_sources(std::vector<std::string> &val, const char *opt, const char *key);
    void update_sources();
    void extract_values(std::vector<spead2::s_item_pointer_t> &val, const std::string &str);
    bool check_header();
    virtual void create_args();
    virtual void apply_header();
  };

}

#endif /* mkrecv_options_h */
