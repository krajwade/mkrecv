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

#include "mkrecv_storage_null.h"
#include "mkrecv_storage_full_dada.h"

#include "mkrecv_receiver_nt.h"

namespace mkrecv
{

  mkrecv::receiver_nt *receiver_nt::instance = NULL;

  static int          g_stopped = 0;
  static std::thread  g_stop_thread;

  void signal_handler(int signalValue)
  {
    std::cout << "received signal " << signalValue << std::endl;
    g_stopped++;
    switch (g_stopped)
      {
      case 1: // first signal: use a thread to stop receiving heaps
	g_stop_thread = std::thread(mkrecv::receiver_nt::request_stop);
	break;
      case 2: // second signal: wait for joining the started thread and exit afterwards
	g_stop_thread.join();
	exit(0);
      default: // third signal: exit immediately
	exit(-1);
      }
  }

  receiver_nt::receiver_nt()
  {
    instance = this;
    opts = NULL;
    storage = NULL;
    thread_pool = NULL;
  }

  receiver_nt::~receiver_nt()
  {
  }

  std::unique_ptr<mkrecv::stream_nt> receiver_nt::make_stream(std::vector<std::string>::iterator first_source,
							      std::vector<std::string>::iterator last_source)
  {
    using asio::ip::udp;

    std::vector<boost::asio::ip::udp::endpoint>  endpoints;
    std::unique_ptr<mkrecv::stream_nt>           stream;

    stream = create_stream();
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

  int receiver_nt::execute(int argc, const char **argv)
  {
    opts = create_options();
    opts->parse_args(argc, argv);
    std::cout << opts->header << std::endl;
    thread_pool.reset(new spead2::thread_pool(opts->threads));
    signal(SIGINT, signal_handler);
    storage = create_storage();
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
	auto &stream = dynamic_cast<mkrecv::stream_nt &>(*ptr);
	n_complete += stream.join();
      }
    g_stop_thread.join();
    std::cout << "Received " << n_complete << " heaps\n";
    return 0;
  }

  std::shared_ptr<mkrecv::options> receiver_nt::create_options()
  {
    return std::shared_ptr<mkrecv::options>(new mkrecv::options());
  }

  std::shared_ptr<mkrecv::storage> receiver_nt::create_storage()
  {
    if (opts->dada_mode == FULL_DADA_MODE)
      {
	return std::shared_ptr<mkrecv::storage>(new storage_null(opts));
      }
    else
      {
	try
	  {
	    return std::shared_ptr<mkrecv::storage>(new storage_full_dada(opts, psrdada_cpp::string_to_key(opts->dada_key), "recv"));
	  }
	catch (std::runtime_error &e)
	  {
	    std::cerr << "Cannot connect to DADA Ringbuffer " << opts->dada_key << " exiting..." << std::endl;
	    exit(0);
	  }
      }
  }

  std::shared_ptr<mkrecv::allocator_nt> receiver_nt::create_allocator()
  {
    return std::shared_ptr<mkrecv::allocator_nt>(new mkrecv::allocator_nt(opts, storage));
  }

  std::unique_ptr<mkrecv::stream_nt> receiver_nt::create_stream()
  {
    spead2::bug_compat_mask bug_compat = opts->pyspead ? spead2::BUG_COMPAT_PYSPEAD_0_5_2 : 0;

    std::shared_ptr<mkrecv::allocator_nt> allocator(create_allocator());
    allocators.push_back(allocator);
    std::unique_ptr<mkrecv::stream_nt> stream(new mkrecv::stream_nt(opts, thread_pool, bug_compat, opts->heaps));
    stream->set_allocator(allocator);
    return stream;
  }

  void receiver_nt::request_stop()
  {
    // request a stop from the allocator (fill the current ringbuffer slot)
    instance->storage->request_stop();
    // test if the last ringbuffer slot was filled and the ringbuffer closed
    do {
      if (instance->storage->is_stopped()) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while (1);
    // And now stop all streams
    for (const auto &ptr : instance->streams)
      {
	auto &stream = dynamic_cast<mkrecv::stream_nt &>(*ptr);
	stream.stop();
      }
  }

}
