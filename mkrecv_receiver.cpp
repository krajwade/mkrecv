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

#include <spead2/common_thread_pool.h>
#include <spead2/recv_udp.h>
#if SPEAD2_USE_NETMAP
# include <spead2/recv_netmap.h>
#endif
#if SPEAD2_USE_IBV
# include <spead2/recv_udp_ibv.h>
#endif

#include "mkrecv_receiver.h"

namespace mkrecv
{

  mkrecv::receiver *receiver::instance = NULL;
  static int g_stopped = 0;

  void signal_handler(int signalValue)
  {
    std::cout << "received signal " << signalValue << std::endl;
    g_stopped++;
    // 1. Set a flag on the memory allocator (we want to finish the current slot or we have to terminate (not sending!) the current slot)
    // 2. If the current slot is finished or terminated, terminate the streams (one for each multicast group)
    mkrecv::receiver::request_stop();
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
	std::string::size_type pos = (*i).find_first_of(":");
	std::string            nwadr = std::string((*i).data(), pos);
	std::string            nwport = std::string((*i).data() + pos + 1, (*i).length() - pos - 1);
	udp::resolver::query   query(nwadr, nwport);
	udp::endpoint          endpoint = *resolver.resolve(query);
	endpoints.push_back(endpoint);
      }
#if SPEAD2_USE_IBV
    if (opts->ibv_if != "")
      {
        boost::asio::ip::address interface_address = boost::asio::ip::address::from_string(opts->ibv_if);
        stream->emplace_reader<spead2::recv::udp_ibv_reader>(endpoints, interface_address, opts->packet, opts->buffer, opts->ibv_comp_vector, opts->ibv_max_poll);
      }
#else
    for (std::vector<boost::asio::ip::udp::endpoint>::iterator it = endpoints.begin() ; it != endpoints.end(); ++it)
      {
#if SPEAD2_USE_NETMAP
        if (opts->netmap_if != "")
	  {
            stream->emplace_reader<spead2::recv::netmap_udp_reader>(opts->netmap_if, (*it).port());
	  }
        else
#endif
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
      }
#endif
    return stream;
  }

  int receiver::execute(int argc, const char **argv)
  {
    opts = create_options();
    opts->parse_args(argc, argv);
    std::cout << opts->header << std::endl;
    thread_pool.reset(new spead2::thread_pool(opts->threads));
    try
      {
	allocator = create_allocator();
      }
    catch (std::runtime_error &e)
      {
	std::cerr << "Cannot connect to DADA Ringbuffer " << opts->key << " exiting..." << std::endl;
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
    
    std::cout << "Received " << n_complete << " heaps\n";
    return 0;
  }

  std::shared_ptr<mkrecv::options> receiver::create_options()
  {
    return std::shared_ptr<mkrecv::options>();
  }

  std::shared_ptr<mkrecv::allocator> receiver::create_allocator()
  {
    return std::shared_ptr<mkrecv::allocator>();
  }

  std::unique_ptr<mkrecv::stream> receiver::create_stream()
  {
    return std::unique_ptr<mkrecv::stream>();
  }

  void receiver::request_stop()
  {
    instance->allocator->request_stop();
  }

}
