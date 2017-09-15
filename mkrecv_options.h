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

namespace po = boost::program_options;

namespace mkrecv
{

  class options
  {
  public:
    bool                      quiet = false;
    bool                      descriptors = false;
    bool                      pyspead = false;
    bool                      joint = false;
    std::size_t               packet = spead2::recv::udp_reader::default_max_size;
    std::string               bind = "0.0.0.0";
    std::size_t               buffer = spead2::recv::udp_reader::default_buffer_size;
    int                       threads = 1;
    std::size_t               heaps = spead2::recv::stream::default_max_heaps;
    bool                      memcpy_nt = false;
#if SPEAD2_USE_NETMAP
    std::string               netmap_if;
#endif
#if SPEAD2_USE_IBV
    std::string               ibv_if;
    int                       ibv_comp_vector = 0;
    int                       ibv_max_poll = spead2::recv::udp_ibv_reader::default_max_poll;
#endif
    std::size_t               freq_first = 0;  // the lowest frequency in all incomming heaps
    std::size_t               freq_step = 1000;   // the difference between consecutive frequencies
    std::size_t               freq_count = 8;  // the number of frequency bands
    std::size_t               feng_first = 0;  // the lowest fengine id
    std::size_t               feng_count = 64;  // the number of fengines
    std::size_t               time_step = 31;   // the difference between consecutive timestamps
    std::size_t               port = 7768;
    std::string               key = "dada";
    std::vector<std::string>  sources;
  private:
    po::options_description              desc;
    po::options_description              hidden;
    po::options_description              all;
    po::positional_options_description   positional;

  public:
    options();
    void usage(std::ostream &o);
    void parse_args(int argc, const char **argv);
  };

}

#endif /* mkrecv_options_h */
