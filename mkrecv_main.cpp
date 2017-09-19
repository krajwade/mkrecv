#include <iostream>
#include <utility>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string>
#include <memory>
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

#include "mkrecv_options.h"
#include "mkrecv_destination.h"
#include "mkrecv_ringbuffer_allocator.h"
#include "mkrecv_fengine_stream.h"

namespace po = boost::program_options;
namespace asio = boost::asio;

namespace mkrecv
{

  template<typename It>
  static std::unique_ptr<fengine_stream> make_stream(
						     spead2::thread_pool &thread_pool, const options &opts,
						     It first_source, It last_source)
  {
    using asio::ip::udp;

    std::unique_ptr<fengine_stream> stream;
    spead2::bug_compat_mask bug_compat = opts.pyspead ? spead2::BUG_COMPAT_PYSPEAD_0_5_2 : 0;
    stream.reset(new fengine_stream(opts, thread_pool, bug_compat, opts.heaps));

    std::shared_ptr<ringbuffer_allocator> dada = std::make_shared<ringbuffer_allocator>(psrdada_cpp::string_to_key(opts.key),
											"recv",
											opts);
    stream->set_ringbuffer(dada);
    if (opts.memcpy_nt)
      stream->set_memcpy(spead2::MEMCPY_NONTEMPORAL);
    for (It i = first_source; i != last_source; ++i)
      {
        udp::resolver resolver(thread_pool.get_io_service());
        udp::resolver::query query(opts.bind, *i);
        udp::endpoint endpoint = *resolver.resolve(query);
#if SPEAD2_USE_NETMAP
        if (opts.netmap_if != "")
	  {
            stream->emplace_reader<spead2::recv::netmap_udp_reader>(
								    opts.netmap_if, endpoint.port());
	  }
        else
#endif
#if SPEAD2_USE_IBV
	  if (opts.ibv_if != "")
	    {
	      boost::asio::ip::address interface_address = boost::asio::ip::address::from_string(opts.ibv_if);
	      stream->emplace_reader<spead2::recv::udp_ibv_reader>(
								   endpoint, interface_address, opts.packet, opts.buffer,
								   opts.ibv_comp_vector, opts.ibv_max_poll);
	    }
	  else
#endif
	    {
	      if (opts.udp_if != "")
		{
		  boost::asio::ip::address interface_address = boost::asio::ip::address::from_string(opts.udp_if);
		  stream->emplace_reader<spead2::recv::udp_reader>(endpoint, opts.packet, opts.buffer, interface_address);
		}
	     else
		{
	      stream->emplace_reader<spead2::recv::udp_reader>(endpoint, opts.packet, opts.buffer);
		}
	    }
      }
    return stream;
  }

}


int main(int argc, const char **argv)
{
  mkrecv::options                                         opts;
  opts.parse_args(argc, argv);
  spead2::thread_pool                                     thread_pool(opts.threads);
  std::vector<std::unique_ptr<mkrecv::fengine_stream> >   streams;
  if (opts.joint)
    {
      streams.push_back(mkrecv::make_stream(thread_pool, opts, opts.sources.begin(), opts.sources.end()));
    }
  else
    {
      for (auto it = opts.sources.begin(); it != opts.sources.end(); ++it)
	streams.push_back(mkrecv::make_stream(thread_pool, opts, it, it + 1));
    }
  
  std::int64_t n_complete = 0;
  for (const auto &ptr : streams)
    {
      auto &stream = dynamic_cast<mkrecv::fengine_stream &>(*ptr);
      n_complete += stream.join();
    }
  
  std::cout << "Received " << n_complete << " heaps\n";
  return 0;
}
