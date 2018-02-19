#include <iostream>
#include <utility>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string>
#include <memory>
#include <signal.h>
#include <atomic>
#include <unordered_map>
#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
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

#include "psrdada_cpp/cli_utils.hpp"
#include "psrdada_cpp/dada_write_client.hpp"
#include "ascii_header.h"

#include "mkrecv_options.h"
#include "mkrecv_destination.h"
#include "mkrecv_ringbuffer_allocator.h"
#include "mkrecv_fengine_stream.h"

namespace po = boost::program_options;
namespace asio = boost::asio;

static int                                           g_stopped = 0;
static std::shared_ptr<mkrecv::ringbuffer_allocator> g_dada;

namespace mkrecv
{

  template<typename It>
  static std::unique_ptr<fengine_stream> make_stream(
						     spead2::thread_pool &thread_pool,
						     options &opts,
						     std::shared_ptr<ringbuffer_allocator> dada,
						     It first_source, It last_source)
  {
    using asio::ip::udp;

    std::unique_ptr<fengine_stream> stream;
    spead2::bug_compat_mask bug_compat = opts.pyspead ? spead2::BUG_COMPAT_PYSPEAD_0_5_2 : 0;
    std::vector<boost::asio::ip::udp::endpoint> endpoints;

    stream.reset(new fengine_stream(opts, thread_pool, bug_compat, opts.heaps));
    stream->set_ringbuffer(dada);
    if (opts.memcpy_nt)
      stream->set_memcpy(spead2::MEMCPY_NONTEMPORAL);
    for (It i = first_source; i != last_source; ++i)
      {
	udp::resolver          resolver(thread_pool.get_io_service());
	std::string::size_type pos = (*i).find_first_of(":");
	std::string            used_source;
	if (pos == std::string::npos)
	  { // no port number given in source string, use opts.port
	    udp::resolver::query query(*i, opts.port);
	    udp::endpoint endpoint = *resolver.resolve(query);
	    endpoints.push_back(endpoint);
	    used_source = *i;
	    used_source.append(":");
	    used_source.append(opts.port);
	  }
	else
	  { // source string contains <ip>:<port> -> split in both parts
	    std::string nwadr = std::string((*i).data(), pos);
	    std::string nwport = std::string((*i).data() + pos + 1, (*i).length() - pos - 1);
	    if (opts.port != "")
	      {
		nwport = opts.port;
	      }
	    udp::resolver::query query(nwadr, nwport);
	    udp::endpoint endpoint = *resolver.resolve(query);
	    endpoints.push_back(endpoint);
	    used_source = nwadr;
	    used_source.append(":");
	    used_source.append(nwport);
	  }
	if (opts.used_sources.length() != 0)
	  {
	    opts.used_sources.append(",");
	  }
	std::cout << "  used source = " << used_source << std::endl;
	opts.used_sources.append(used_source);
      }
#if SPEAD2_USE_IBV
    if (opts.ibv_if != "")
      {
        boost::asio::ip::address interface_address = boost::asio::ip::address::from_string(opts.ibv_if);
        stream->emplace_reader<spead2::recv::udp_ibv_reader>(endpoints, interface_address, opts.packet, opts.buffer, opts.ibv_comp_vector, opts.ibv_max_poll);
      }
#else
    for (std::vector<boost::asio::ip::udp::endpoint>::iterator it = endpoints.begin() ; it != endpoints.end(); ++it)
      {
#if SPEAD2_USE_NETMAP
        if (opts.netmap_if != "")
	  {
            stream->emplace_reader<spead2::recv::netmap_udp_reader>(opts.netmap_if, (*it).port());
	  }
        else
#endif
	  {
	    if (opts.udp_if != "")
	      {
		boost::asio::ip::address interface_address = boost::asio::ip::address::from_string(opts.udp_if);
		stream->emplace_reader<spead2::recv::udp_reader>(*it, opts.packet, opts.buffer, interface_address);
	      }
	    else
	      {
		stream->emplace_reader<spead2::recv::udp_reader>(*it, opts.packet, opts.buffer);
	      }
	  }
      }
#endif
    return stream;
  }

}

void signal_handler(int signalValue)
{
  std::cout << "received signal " << signalValue << std::endl;
  g_stopped++;
  // 1. Set a flag on the memory allocator (we want to finish the current slot or we have to terminate (not sending!) the current slot)
  // 2. If the current slot is finished or terminated, terminate the streams (one for each multicast group)
  g_dada->requestStop();
  if (g_stopped > 1)
    {
      exit(-1);
    }
}

int main(int argc, const char **argv)
{
  mkrecv::options                                         opts;
  opts.parse_args(argc, argv);
  opts.set_start_time(13);
  spead2::thread_pool                                     thread_pool(opts.threads);
  g_dada = std::make_shared<mkrecv::ringbuffer_allocator>(psrdada_cpp::string_to_key(opts.key), "recv", &opts);
  std::vector<std::unique_ptr<mkrecv::fengine_stream> >   streams;
  signal(SIGINT, signal_handler);
  if (opts.joint)
    {
      streams.push_back(mkrecv::make_stream(thread_pool, opts, g_dada, opts.sources.begin(), opts.sources.end()));
    }
  else
    {
      for (auto it = opts.sources.begin(); it != opts.sources.end(); ++it)
	streams.push_back(mkrecv::make_stream(thread_pool, opts, g_dada, it, it + 1));
    }

  ascii_header_set(opts.header, SOURCES_KEY, "%s", opts.used_sources.c_str());
  opts.check_header();
  std::cout << opts.header << std::endl;

  std::int64_t n_complete = 0;
  for (const auto &ptr : streams)
    {
      auto &stream = dynamic_cast<mkrecv::fengine_stream &>(*ptr);
      n_complete += stream.join();
    }
  
  std::cout << "Received " << n_complete << " heaps\n";
  return 0;
}
