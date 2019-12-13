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

//#define COMBINED_IP_PORT

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
#define QUIET_DESC         "Only show total of heaps received"

#define DESCRIPTORS_OPT    "descriptors"
#define DESCRIPTORS_DESC   "Show descriptors"

#define PYSPEAD_OPT        "pyspead"
#define PYSPEAD_DESC       "Be bug-compatible with PySPEAD"

#define JOINT_OPT          "joint"
#define JOINT_DESC         "Treat all sources as a single stream"

/* the following options should have sufficient default values */
#define PACKET_OPT         "packet-size"
#define PACKET_KEY         "PACKET_SIZE"
#define PACKET_DESC        "Maximum packet size to use for UDP"

#define BUFFER_OPT         "buffer-size"
#define BUFFER_KEY         "BUFFER_SIZE"
#define BUFFER_DESC        "Socket buffer size"

#define NTHREADS_OPT       "nthreads"
#define NTHREADS_KEY       "NTHREADS"
#define NTHREADS_DESC      "Number of worker threads"

#define NHEAPS_OPT         "nheaps"
#define NHEAPS_KEY         "NHEAPS"
#define NHEAPS_DESC        "Maximum number of in-flight heaps"

/* The following options describe the DADA ringbuffer use */

#define DADA_MODE_OPT      "dada-mode"
#define DADA_MODE_KEY      "DADA_MODE"
#define DADA_MODE_DESC     "dada mode (0 = no, 1 = huge trash, 2 = dada, 3 = dada+slot, 4 = full dada)"
#define NO_DADA_MODE       0
#define TRASH_DADA_MODE    1
#define STATIC_DADA_MODE   2
#define DYNAMIC_DADA_MODE  3
#define FULL_DADA_MODE     4

#define DADA_KEY_OPT       "dada-key"
#define DADA_KEY_KEY       "DADA_KEY"
#define DADA_KEY_DESC      "PSRDADA ring buffer key"

#define DADA_NSLOTS_OPT    "dada-nslots"
#define DADA_NSLOTS_KEY    "DADA_NSLOTS"
#define DADA_NSLOTS_DESC   "Maximum number of open dada ringbuffer slots"

/* The following options describe the connection to the network */

#if SPEAD2_USE_IBV
#define IBV_IF_OPT         "ibv-if"
#define IBV_IF_KEY         "IBV_IF"
#define IBV_IF_DESC        "Interface address for ibverbs"

#define IBV_VECTOR_OPT     "ibv-vector"
#define IBV_VECTOR_KEY     "IBV_VECTOR"
#define IBV_VECTOR_DESC    "Interrupt vector (-1 for polled)"

#define IBV_MAX_POLL_OPT   "ibv-max-poll"
#define IBV_MAX_POLL_KEY   "IBV_MAX_POLL"
#define IBV_MAX_POLL_DESC  "Maximum number of times to poll in a row"
#endif

#define UDP_IF_OPT         "udp-if"
#define UDP_IF_KEY         "UDP_IF"
#define UDP_IF_DESC        "UDP interface"

#define PORT_OPT           "port"
#define PORT_KEY           "PORT"
#define PORT_DESC          "Port number"

#define SOURCES_OPT        "source"
#define SOURCES_KEY        "MCAST_SOURCES"
#define SOURCES_DESC       "sources"

/* The following options describe the timing information */
#define SYNC_EPOCH_OPT     "sync-epoch"
#define SYNC_EPOCH_KEY     "SYNC_TIME"
#define SYNC_EPOCH_DESC    "the ADC sync epoch"

#define SAMPLE_CLOCK_OPT   "sample-clock"
#define SAMPLE_CLOCK_KEY   "SAMPLE_CLOCK"
#define SAMPLE_CLOCK_DESC  "virtual sample clock used for calculations"

/* The following options describe the item -> index mapping */

#define NINDICES_OPT       "nindices"
#define NINDICES_KEY       "NINDICES"
#define NINDICES_DESC      "Number of item pointers used as index"

#define IDX_ITEM_OPT      "idx%d-item"
#define IDX_ITEM_KEY      "IDX%d_ITEM"
#define IDX_ITEM_DESC     "Item pointer index for index component %d"

#define IDX_STEP_OPT      "idx1-step"
#define IDX_STEP_KEY      "IDX1_STEP"
#define IDX_STEP_DESC     "The difference between successive timestamps"

#define IDX_MOD_OPT       "idx1-modulo"
#define IDX_MOD_KEY       "IDX1_MODULO"
#define IDX_MOD_DESC      "Sets the first timestamp to a multiple of this value"

#define IDX_MASK_OPT      "idx%d-mask"
#define IDX_MASK_KEY      "IDX%d_MASK"
#define IDX_MASK_DESC     "Mask for using only a part of an item value for index component %d"

#define IDX_LIST_OPT      "idx%d-list"
#define IDX_LIST_KEY      "IDX%d_LIST"
#define IDX_LIST_DESC     "A List of item pointer values for index component %d"

/* It is possible to specify the heap size and use it as a filter, otherwise the first heap is used to determine this size. */
#define HEAP_SIZE_OPT      "heap-size"
#define HEAP_SIZE_KEY      "HEAP_SIZE"
#define HEAP_SIZE_DESC     "The heap size used for checking incomming heaps."

#define NGROUPS_DATA_OPT   "ngroups-data"
#define NGROUPS_DATA_KEY   "NGROUPS_DATA"
#define NGROUPS_DATA_DESC  "Number of groups (heaps with the same timestamp) going into the data space."

#define NGROUPS_TEMP_OPT   "ngroups-temp"
#define NGROUPS_TEMP_KEY   "NGROUPS_TEMP"
#define NGROUPS_TEMP_DESC  "Number of groups (heaps with the same timestamp) going into the temporary space."

#define LEVEL_DATA_OPT     "level-data"
#define LEVEL_DATA_KEY     "LEVEL_DATA"
#define LEVEL_DATA_DESC    "Fill level of the data destination before switching to sequential mode (percent)."

#define LEVEL_TEMP_OPT     "level-temp"
#define LEVEL_TEMP_KEY     "LEVEL_TEMP"
#define LEVEL_TEMP_DESC    "Fill level of the temporary destination before switching to parallel mode (percent)."

#define NHEAPS_SWITCH_OPT   "nheaps-switch"
#define NHEAPS_SWITCH_KEY   "NHEAPS_SWITCH"
#define NHEAPS_SWITCH_DESC  "Number of received heaps before switching to a new ringbuffer slot."

/* It is possible to put a list of item pointer values into some kind of side-channel inside a ringbuffer slot */
#define SCI_LIST_OPT       "sci-list"
#define SCI_LIST_KEY       "SCI_LIST"
#define SCI_LIST_DESC      "list of item pointers going into a side-channel inside a ringbuffer slot"

namespace po = boost::program_options;

namespace mkrecv
{

  static constexpr char  ASCII_HEADER_SENTINEL = 4;
  typedef enum {DEFAULT_USED, CONFIG_USED, OPTION_USED} USED_TYPE;

  class index_options
  {
  public:
    std::string                           item_str       = "0";
    USED_TYPE                             item_used_type = DEFAULT_USED;
    std::size_t                           item           =  0;  // IDXi_ITEM
    std::string                           mask_str       = "0xffffffffffff";
    USED_TYPE                             mask_used_type = DEFAULT_USED;
    std::size_t                           mask           = 0xffffffffffff;  // IDXi_MASK
    std::string                           step_str       = "0";
    USED_TYPE                             step_used_type = DEFAULT_USED;
    std::size_t                           step           =  0;  // IDXi_STEP
    std::string                           mod_str        = "0";
    USED_TYPE                             mod_used_type  = DEFAULT_USED;
    std::size_t                           mod            =  0;  // IDXi_MODULO
    std::string                           list           = "";  // IDXi_LIST
    USED_TYPE                             list_used_type = DEFAULT_USED;
    std::vector<spead2::s_item_pointer_t> values;
  };

  class options
  {
  protected:
    bool                      ready = false;
  public:
    // optional header file contain configuration options and additional information
    std::string               hdrname             = HEADER_DEF;
    // some flags
    bool                      quiet               = false;
    bool                      descriptors         = false;
    bool                      pyspead             = false;
    bool                      joint               = false;
    // some options, default values should be ok to use, will _not_ go into header
    std::string               packet_str          = "9200";
    std::size_t               packet              =  9200;
    std::string               buffer_str          = "8388608";
    std::size_t               buffer              =  8388608;
    std::string               threads_str         = "1";
    int                       threads             =  1;
    std::string               heaps_str           = "64";
    std::size_t               heaps               =  64;
    bool                      memcpy_nt           = false;
    // DADA ringbuffer related stuff
    std::string               dada_mode_str       = "4";
    std::size_t               dada_mode           =  4;
    std::string               dada_key            = "dada";
    std::string               dada_nslots_str     = "2";
    std::size_t               dada_nslots         =  2;
    // network configuration
#if SPEAD2_USE_IBV
    std::string               ibv_if              = "";
    std::string               ibv_comp_vector_str = "0";
    int                       ibv_comp_vector     =  0;
    std::string               ibv_max_poll_str    = "10";
    int                       ibv_max_poll        =  10;
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
    int                       nindices            =  0;
    index_options             indices[MAX_INDEXPARTS];
    std::string               heap_size_str       = "0";
    std::size_t               heap_size           =  0;
    std::string               ngroups_data_str    = "64";
    std::size_t               ngroups_data        =  64;
    std::string               ngroups_temp_str    = "32";
    std::size_t               ngroups_temp        =  32;
    std::string               level_data_str      = "50";
    std::size_t               level_data          =  50;
    std::string               level_temp_str      = "50";
    std::size_t               level_temp          =  50;
    std::string               nheaps_switch_str   = "0";
    std::size_t               nheaps_switch       =  0;
    std::string               sci_list            = "";
    std::size_t               nsci                = 0;
    std::vector<std::size_t>  scis;
    // heap filter mechanism
    char                     *header              = NULL;
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
  protected:
    USED_TYPE finalize_parameter(std::string &val_str, const char *opt, const char *key);
    bool parse_fixnum(int &val, std::string &val_str);
    bool parse_fixnum(spead2::s_item_pointer_t &val, std::string &val_str);
    bool parse_fixnum(std::size_t &val, std::string &val_str);
    USED_TYPE parse_parameter(std::string &val, const char *opt, const char *key);
    USED_TYPE parse_parameter(int &val, std::string &val_str, const char *opt, const char *key);
    USED_TYPE parse_parameter(std::size_t &val, std::string &val_str, const char *opt, const char *key);
    USED_TYPE parse_parameter(double &val, std::string &val_str, const char *opt, const char *key);
    USED_TYPE parse_parameter(bool &val, std::string &val_str, const char *opt, const char *key);
    USED_TYPE parse_parameter(std::vector<spead2::s_item_pointer_t> &val, std::string &val_str, const char *opt, const char *key);
    USED_TYPE parse_parameter(std::vector<std::size_t> &val, std::string &val_str, const char *opt, const char *key);
    USED_TYPE parse_parameter(std::vector<std::string> &val, std::string &val_str, const char *opt, const char *key);
    bool check_index_specification();
  };

}

#endif /* mkrecv_options_h */
