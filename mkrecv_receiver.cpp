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
#include <thread>

#include <spead2/common_thread_pool.h>
#include <spead2/recv_udp.h>
#if SPEAD2_USE_IBV
# include <spead2/recv_udp_ibv.h>
#endif

#include "psrdada_cpp/cli_utils.hpp"

#include "mkrecv_receiver.h"

namespace mkrecv
{

  mkrecv::receiver *receiver::instance = NULL;

  static int          g_stopped = 0;
  static std::thread  g_stop_thread;

  void signal_handler(int signalValue)
  {
    std::cout << "received signal " << signalValue << '\n';
    g_stopped++;
    // 1. Set a flag on the memory allocator (we want to finish the current slot or we have to terminate (not sending!) the current slot)
    // 2. If the current slot is finished or terminated, terminate the streams (one for each multicast group)
    g_stop_thread = std::thread(mkrecv::receiver::request_stop);
    if (g_stopped > 1)
      {
	exit(-1);
      }
  }

  receiver::receiver()
  {
    instance = this;
    opts = NULL;
    allocator = NULL;
    thread_pool = NULL;
  }

  receiver::~receiver()
  {
  }

  std::unique_ptr<mkrecv::stream> receiver::make_stream(std::vector<std::string>::iterator first_source,
							std::vector<std::string>::iterator last_source)
  {
    using asio::ip::udp;

    std::vector<boost::asio::ip::udp::endpoint>  endpoints;
    std::unique_ptr<mkrecv::stream>              stream;

    stream = create_stream();
    stream->set_ringbuffer(allocator);
    if (opts->memcpy_nt)
      stream->set_memcpy(spead2::MEMCPY_NONTEMPORAL);
    for (std::vector<std::string>::iterator i = first_source; i != last_source; ++i)
      {
	udp::resolver          resolver(thread_pool->get_io_service());
	udp::resolver::query   query((*i), opts->port);
	udp::endpoint          endpoint = *resolver.resolve(query);
	//std::cout << "make stream for " << (*i) << ":" << opts->port << '\n';
	endpoints.push_back(endpoint);
      }
#if SPEAD2_USE_IBV
    if (opts->ibv_if != "")
      {
        boost::asio::ip::address interface_address = boost::asio::ip::address::from_string(opts->ibv_if);
        stream->emplace_reader<spead2::recv::udp_ibv_reader>(endpoints, interface_address, opts->packet, opts->buffer, opts->ibv_comp_vector, opts->ibv_max_poll);
      }
    else
      {
#endif
	for (std::vector<boost::asio::ip::udp::endpoint>::iterator it = endpoints.begin() ; it != endpoints.end(); ++it)
	  {
	    if (opts->udp_if != "")
	      {
		boost::asio::ip::address interface_address = boost::asio::ip::address::from_string(opts->udp_if);
		stream->emplace_reader<spead2::recv::udp_reader>(*it, opts->packet, opts->buffer, interface_address);
	      }
	    else
	      {
		stream->emplace_reader<spead2::recv::udp_reader>(*it, opts->packet, opts->buffer);
	      }
	  }
#if SPEAD2_USE_IBV
      }
#endif
    return stream;
  }

  int receiver::execute(int argc, const char **argv)
  {
    opts = create_options();
    opts->parse_args(argc, argv);
    std::cout << opts->header << '\n';
    thread_pool.reset(new spead2::thread_pool(opts->threads));
    try
      {
	allocator = create_allocator();
      }
    catch (std::runtime_error &e)
      {
	std::cerr << "Cannot connect to DADA Ringbuffer " << opts->dada_key << " exiting..." << '\n';
	exit(0);
      }
    signal(SIGINT, signal_handler);
    if (opts->joint)
      {
	streams.push_back(make_stream(opts->sources.begin(), opts->sources.end()));
      }
    else
      {
	for (auto it = opts->sources.begin(); it != opts->sources.end(); ++it)
	  streams.push_back(make_stream(it, it + 1));
      }
    std::int64_t n_complete = 0;
    for (const auto &ptr : streams)
      {
	auto &stream = dynamic_cast<mkrecv::stream &>(*ptr);
	n_complete += stream.join();
      }
    g_stop_thread.join();
    std::cout << "Received " << n_complete << " heaps\n";
    return 0;
  }

  std::shared_ptr<mkrecv::options> receiver::create_options()
  {
    return std::shared_ptr<mkrecv::options>(new mkrecv::options());
  }

  std::shared_ptr<mkrecv::allocator> receiver::create_allocator()
  {
    return std::shared_ptr<mkrecv::allocator>(new mkrecv::allocator(psrdada_cpp::string_to_key(opts->dada_key), "recv", opts));
  }

  std::unique_ptr<mkrecv::stream> receiver::create_stream()
  {
    spead2::bug_compat_mask bug_compat = opts->pyspead ? spead2::BUG_COMPAT_PYSPEAD_0_5_2 : 0;

    return std::unique_ptr<mkrecv::stream>(new mkrecv::stream(opts, thread_pool, bug_compat, opts->heaps));
  }

  void receiver::request_stop()
  {
    // request a stop from the allocator (fill the current ringbuffer slot)
    instance->allocator->request_stop();
    // test if the last ringbuffer slot was filled and the ringbuffer closed
    do {
      if (instance->allocator->is_stopped()) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while (1);
    // And now stop all streams
    for (const auto &ptr : instance->streams)
      {
	auto &stream = dynamic_cast<mkrecv::stream &>(*ptr);
	stream.stop();
      }
  }

}
