#ifndef mkrecv_options_h
#define mkrecv_options_h

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <boost/program_options.hpp>

#include <spead2/recv_udp.h>
#if SPEAD2_USE_NETMAP
# include <spead2/recv_netmap.h>
#endif
#if SPEAD2_USE_IBV
# include <spead2/recv_udp_ibv.h>
#endif

#include "dada_def.h"

#define CLOCK_RATE 
#define DADA_TIMESTR "%Y-%m-%d-%H:%M:%S"

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

/* The following options influence directly the behaviour as heap filter */
#define FREQ_FIRST_OPT     "freq_first"
#define FREQ_FIRST_KEY     "FREQ_FIRST"
#define FREQ_FIRST_DESC    "lowest frequency in all incomming heaps"
#define FREQ_FIRST_DEF     0

#define FREQ_STEP_OPT      "freq_step"
#define FREQ_STEP_KEY      "FREQ_STEP"
#define FREQ_STEP_DESC     "difference between consecutive frequencies"
#define FREQ_STEP_DEF      512

#define FREQ_COUNT_OPT     "freq_count"
#define FREQ_COUNT_KEY     "FREQ_COUNT"
#define FREQ_COUNT_DESC    "number of frequency bands"
#define FREQ_COUNT_DEF     8

#define FENG_FIRST_OPT     "feng_first"
#define FENG_FIRST_KEY     "FIRST_ANT"
#define FENG_FIRST_DESC    "lowest fengine id"
#define FENG_FIRST_DEF     0

#define FENG_COUNT_OPT     "feng_count"
#define FENG_COUNT_KEY     "NANTS"
#define FENG_COUNT_DESC    "number of fengines"
#define FENG_COUNT_DEF     0

#define SAMPLE_CLOCK_OPT   "sample_clock"
#define SAMPLE_CLOCK_KEY   "SAMPLE_CLOCK"
#define SAMPLE_CLOCK_DESC  "virtual sample clock used for calculations"
#define SAMPLE_CLOCK_DEF   1750000000.0

#define SYNC_EPOCH_OPT     "sync_epoch"
#define SYNC_EPOCH_KEY     "SYNC_TIME"
#define SYNC_EPOCH_DESC    "the ADC sync epoch"
#define SYNC_EPOCH_DEF     0.0

#define TIME_STEP_OPT      "time_step"
#define TIME_STEP_KEY      "TIME_STEP"
#define TIME_STEP_DESC     "difference between consecutive timestamps"
#define TIME_STEP_DEF      0x200000   // 2^21

#define SAMPLE_CLOCK_START_KEY  "SAMPLE_CLOCK_START"
#define UTC_START_KEY           "UTC_START"

namespace po = boost::program_options;

namespace mkrecv
{

  static const char  ASCII_HEADER_SENTINEL = 4;

  class options
  {
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
    // heap filter mechanism
    std::size_t               freq_first      = FREQ_FIRST_DEF;
    std::size_t               freq_step       = FREQ_STEP_DEF;
    std::size_t               freq_count      = FREQ_COUNT_DEF;
    std::size_t               feng_first      = FENG_FIRST_DEF;
    std::size_t               feng_count      = FENG_COUNT_DEF;
    double                    sample_clock    = SAMPLE_CLOCK_DEF;
    double                    sync_epoch      = SYNC_EPOCH_DEF;
    std::size_t               time_step       = TIME_STEP_DEF;
    char                     *header;
  private:
    po::options_description              desc;
    po::options_description              hidden;
    po::options_description              all;
    po::positional_options_description   positional;
    po::variables_map                    vm;

  public:
    options();
    ~options();
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
    void apply_header();
    bool check_header();
  };

}

#endif /* mkrecv_options_h */
