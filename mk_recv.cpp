
/*

g++ -DNDEBUG -I. -I/usr/include -I$HOME/include -std=c++11 -O3 -MT mk_recv.o -MD -MP -c -o mk_recv.o mk_recv.cpp
g++ -std=c++11 -O3 -o mk_recv mk_recv.o $HOME/lib/libpsrdada_cpp.a $HOME/lib/libspead2.a $HOME/lib/libpsrdada.a -lboost_system -lboost_program_options -lboost_log -lpthread

g++ -DNDEBUG -I. -I/usr/include -I$HOME/linux/include -std=c++11 -O3 -MT mk_recv.o -MD -MP -c -o mk_recv.o mk_recv.cpp
g++ -std=c++11 -O3 -o mk_recv mk_recv.o $HOME/linux/lib/libpsrdada_cpp.a $HOME/linux/lib/libspead2.a $HOME/linux/lib/libpsrdada.a -lboost_system -lboost_program_options -lboost_log -lpthread -libverbs -lrdmacm


 */

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

namespace po = boost::program_options;
namespace asio = boost::asio;

const unsigned int TIMESTAMP_ID   = 0x1600;
const unsigned int FENG_ID_ID     = 0x4101;
const unsigned int FREQUENCY_ID   = 0x4103;
const unsigned int FENG_RAW_ID    = 0x4300;

const std::size_t  MAX_TEMPORARY_SPACE = 8*64*4096*2*2;

struct header
{
  spead2::s_item_pointer_t   timestamp;
};

struct options
{
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
};

typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;

static time_point start = std::chrono::high_resolution_clock::now();



static void usage(std::ostream &o, const po::options_description &desc)
{
  o << "Usage: spead2_recv [options] <port>\n";
  o << desc;
}

template<typename T>
static po::typed_value<T> *make_opt(T &var)
{
  return po::value<T>(&var)->default_value(var);
}

static po::typed_value<bool> *make_opt(bool &var)
{
  return po::bool_switch(&var)->default_value(var);
}

static options parse_args(int argc, const char **argv)
{
  options opts;
  po::options_description desc, hidden, all;
  desc.add_options()
    ("quiet", make_opt(opts.quiet), "Only show total of heaps received")
    ("descriptors", make_opt(opts.descriptors), "Show descriptors")
    ("pyspead", make_opt(opts.pyspead), "Be bug-compatible with PySPEAD")
    ("joint", make_opt(opts.joint), "Treat all sources as a single stream")
    ("packet", make_opt(opts.packet), "Maximum packet size to use for UDP")
    ("bind", make_opt(opts.bind), "Bind socket to this hostname")
    ("buffer", make_opt(opts.buffer), "Socket buffer size")
    ("threads", make_opt(opts.threads), "Number of worker threads")
    ("heaps", make_opt(opts.heaps), "Maximum number of in-flight heaps")
    ("memcpy-nt", make_opt(opts.memcpy_nt), "Use non-temporal memcpy")
#if SPEAD2_USE_NETMAP
    ("netmap", make_opt(opts.netmap_if), "Netmap interface")
#endif
#if SPEAD2_USE_IBV
    ("ibv", make_opt(opts.ibv_if), "Interface address for ibverbs")
    ("ibv-vector", make_opt(opts.ibv_comp_vector), "Interrupt vector (-1 for polled)")
    ("ibv-max-poll", make_opt(opts.ibv_max_poll), "Maximum number of times to poll in a row")
#endif
    ("freq_first", make_opt(opts.freq_first), "lowest frequency in all incomming heaps")
    ("freq_step", make_opt(opts.freq_step), "difference between consecutive frequencies")
    ("freq_count", make_opt(opts.freq_count), "number of frequency bands")
    ("feng_first", make_opt(opts.feng_first), "lowest fengine id")
    ("feng_count", make_opt(opts.feng_count), "number of fengines")
    ("time_step", make_opt(opts.time_step), "difference between consecutive timestamps")
    ("key", make_opt(opts.key), "PSRDADA ring buffer key")
    ("port", make_opt(opts.port), "Port number")
    ;
  hidden.add_options()
    ("source", po::value<std::vector<std::string>>()->composing(), "sources");
  all.add(desc);
  all.add(hidden);

  po::positional_options_description positional;
  positional.add("source", -1);
  try
    {
      po::variables_map vm;
      po::store(po::command_line_parser(argc, argv)
		.style(po::command_line_style::default_style & ~po::command_line_style::allow_guessing)
		.options(all)
		.positional(positional)
		.run(), vm);
      po::notify(vm);
      if (vm.count("help"))
        {
	  usage(std::cout, desc);
	  std::exit(0);
        }
      if (!vm.count("source"))
	throw po::error("At least one IP is required");
      opts.sources = vm["source"].as<std::vector<std::string>>();
#if SPEAD2_USE_NETMAP
      if (opts.sources.size() > 1 && opts.netmap_if != "")
        {
	  throw po::error("--netmap cannot be used with multiple sources");
        }
#endif
      return opts;
    }
  catch (po::error &e)
    {
      std::cerr << e.what() << '\n';
      usage(std::cerr, desc);
      std::exit(2);
    }
}

void show_heap(const spead2::recv::heap &fheap, const options &opts)
{
  if (opts.quiet)
    return;
  time_point now = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = now - start;
  std::cout << std::showbase;
  std::cout << "Received heap " << fheap.get_cnt() << " at " << elapsed.count() << '\n';
  if (opts.descriptors)
    {
      std::vector<spead2::descriptor> descriptors = fheap.get_descriptors();
      for (const auto &descriptor : descriptors)
        {
	  std::cout
	    << "Descriptor for " << descriptor.name
	    << " (" << std::hex << descriptor.id << ")\n"
	    << "  description: " << descriptor.description << '\n'
	    << "  format:      [";
	  bool first = true;
	  for (const auto &field : descriptor.format)
            {
	      if (!first)
		std::cout << ", ";
	      first = false;
	      std::cout << '(' << field.first << ", " << field.second << ')';
            }
	  std::cout << "]\n";
	  std::cout
	    << "  dtype:       " << descriptor.numpy_header << '\n'
	    << "  shape:       (";
	  first = true;
	  for (const auto &size : descriptor.shape)
            {
	      if (!first)
		std::cout << ", ";
	      first = false;
	      if (size == -1)
		std::cout << "?";
	      else
		std::cout << size;
            }
	  std::cout << ")\n";
        }
    }
  const auto &items = fheap.get_items();
  for (const auto &item : items)
    {
      std::cout << std::hex << item.id << std::dec
		<< " = [" << item.length << " bytes]\n";
    }
  std::cout << std::noshowbase;
}

const int DATA_DEST  = 0;
const int TEMP_DEST  = 1;
const int TRASH_DEST = 2;

const int INIT_STATE       = 0;
const int SEQUENTIAL_STATE = 1;
const int PARALLEL_STATE   = 2;

class destination
{
public:
  psrdada_cpp::RawBytes    ptr;
  std::size_t              size = 0;     // destination size in bytes
  std::size_t              capacity = 0; // the number of timestamps which fits into the destination
  std::size_t              first = 0;    // the first timestamp in a heap destination
  std::size_t              space = 0;    // number of heaps which can go into this destination
  std::size_t              needed = 0;   // number of heaps needed until the destination is full
  destination();
  void set_buffer(psrdada_cpp::RawBytes &ptr, std::size_t size);
  void allocate_buffer(std::size_t size);
};

destination::destination() : ptr(NULL,0) {}

void destination::set_buffer(psrdada_cpp::RawBytes &ptr, std::size_t size)
{
  this->size = size;
  this->ptr = ptr;
}

void destination::allocate_buffer(std::size_t size)
{
  this->size = size;
  this->ptr  = psrdada_cpp::RawBytes(new char[size], size, 0);
}

class ringbuffer_allocator : public spead2::memory_allocator
{
private:
  /// Mutex protecting @ref allocator
  psrdada_cpp::MultiLog         mlog;
  psrdada_cpp::DadaWriteClient  dada;
  psrdada_cpp::RawBytes         hdr;   // memory to store constant (header) information
  std::mutex                    dest_mutex;
  destination                   dest[3];
  std::unordered_map<spead2::s_item_pointer_t, int> heap2dest;
  std::size_t                   heap_size;   // Size of the HEAP payload (TPC), s_item_pointer_t, ph->payload_length)
  std::size_t                   freq_size;   //
  std::size_t                   freq_first;  // the lowest frequency in all incomming heaps
  std::size_t                   freq_step;   // the difference between consecutive frequencies
  std::size_t                   freq_count;  // the number of frequency bands
  std::size_t                   feng_size;   // freq_count*freq_size
  std::size_t                   feng_first;  // the lowest fengine id
  std::size_t                   feng_count;  // the number of fengines
  std::size_t                   time_size;   // feng_count*feng_size
  std::size_t                   time_step;   // the difference between consecutive timestamps
  int                           state = INIT_STATE;
  std::size_t                   ntrash = 0;
  std::size_t                   nlost = 0;

public:
  ringbuffer_allocator(key_t key, std::string mlname, const options &opts);
  ~ringbuffer_allocator();
  virtual spead2::memory_allocator::pointer allocate(std::size_t size, void *hint) override;

private:
  virtual void free(std::uint8_t *ptr, void *user) override;

public:
  void handle_data_full();
  void handle_temp_full();
  void mark(spead2::recv::heap &heap);
};

ringbuffer_allocator::ringbuffer_allocator(key_t key, std::string mlname, const options &opts) :
  mlog(mlname),
  dada(key, mlog),
  hdr(NULL, 0)
{
  int i;

  hdr = dada.header_stream().next();
  dest[DATA_DEST].set_buffer(dada.data_stream().next(), dada.data_buffer_size());
  dest[TEMP_DEST].allocate_buffer(MAX_TEMPORARY_SPACE);
  dest[TRASH_DEST].allocate_buffer(MAX_TEMPORARY_SPACE);
  freq_first = opts.freq_first;  // the lowest frequency in all incomming heaps
  freq_step  = opts.freq_step;   // the difference between consecutive frequencies
  freq_count = opts.freq_count;  // the number of frequency bands
  feng_first = opts.feng_first;  // the lowest fengine id
  feng_count = opts.feng_count;  // the number of fengines
  time_step  = opts.time_step;   // the difference between consecutive timestamps
}

ringbuffer_allocator::~ringbuffer_allocator()
{
  std::cout << "ntrash " << ntrash << '\n';
  std::cout << "nlost " << nlost << '\n';
  delete dest[DATA_DEST].ptr.ptr();
  delete dest[TRASH_DEST].ptr.ptr();
}

spead2::memory_allocator::pointer ringbuffer_allocator::allocate(std::size_t size, void *hint)
{
  spead2::recv::packet_header    *ph = (spead2::recv::packet_header*)hint;
  spead2::recv::pointer_decoder   decoder(ph->heap_address_bits);
  spead2::s_item_pointer_t        timestamp, feng_id, frequency;
  std::size_t                     time_index;
  char*                           mem_base = NULL;
  std::size_t                     mem_offset;
  int                             d;

  // extract some values from the heap
  for (int i = 0; i < ph->n_items; i++)
    {
      spead2::item_pointer_t pointer = spead2::load_be<spead2::item_pointer_t>(ph->pointers + i * sizeof(spead2::item_pointer_t));
      bool special;
      if (decoder.is_immediate(pointer))
        {
	  switch (decoder.get_id(pointer))
            {
            case TIMESTAMP_ID:
	      timestamp = decoder.get_immediate(pointer);
	      break;
            case FENG_ID_ID:
	      feng_id = decoder.get_immediate(pointer);
	      break;
            case FREQUENCY_ID:
	      frequency = decoder.get_immediate(pointer);
	      break;
            default:
	      break;
            }
        }
    }
  // put some values in the header buffer
  if (state == INIT_STATE)
    { // put some values in the header buffer
      header  h;
      h.timestamp = timestamp;
      memcpy(hdr.ptr(), &h, sizeof(h));
      hdr.used_bytes(sizeof(h));
      dada.header_stream().release();
      // calculate some values needed to calculate a memory offset for incoming heaps
      heap_size = ph->payload_length;
      freq_size = heap_size;
      feng_size = freq_count*freq_size;
      time_size = feng_count*feng_size;
      dest[DATA_DEST].capacity  = dest[DATA_DEST].size/time_size;
      dest[TEMP_DEST].capacity  = dest[TEMP_DEST].size/time_size;
      dest[TRASH_DEST].capacity = dest[TRASH_DEST].size/time_size;
      dest[DATA_DEST].first  = timestamp;
      dest[DATA_DEST].space  = dest[DATA_DEST].capacity*feng_count*freq_count;
      dest[DATA_DEST].needed = dest[DATA_DEST].space;
      dest[TEMP_DEST].space  = dest[TEMP_DEST].capacity*feng_count*freq_count;
      dest[TEMP_DEST].needed = dest[TEMP_DEST].space;
      state = SEQUENTIAL_STATE;
    }
  // The term "+ time_step/2" allows to have nonintegral time_steps.
  time_index = (timestamp - dest[DATA_DEST].first + time_step/2)/time_step;
  if (state == SEQUENTIAL_STATE)
    { // data and temp are in sequential order
      if (time_index < 0)
	{ // a negative index is for heaps before the first one -> thrash
	  d = TRASH_DEST;
	  time_index = 0;
	  ntrash++;
	}
      else if (time_index >= dest[DATA_DEST].capacity + dest[TEMP_DEST].capacity)
	{ // ERROR: we are not fast enough to keep up with the data stream
	  d = TRASH_DEST;
	  time_index = 0;
	  nlost++;
	}
      else if (time_index < dest[DATA_DEST].capacity)
	{ // the current heap will go into the ringbuffer slot
	  d = DATA_DEST;
	}
      else
	{ // the current heap will go into the temporary memory
	  // determine the first timestamp in the temporary buffer
	  if (dest[TEMP_DEST].first == 0) dest[TEMP_DEST].first = timestamp;
	  if (dest[TEMP_DEST].first > timestamp) dest[TEMP_DEST].first = timestamp;
	  d = TEMP_DEST;
	  time_index -= dest[DATA_DEST].capacity;
	}
    }
  else if (state == PARALLEL_STATE)
    { // data and temp are in parallel order
      if (time_index < 0)
	{ // a negative index is for heaps before the first one -> thrash
	  d = TRASH_DEST;
	  time_index = 0;
	  ntrash++;
	}
      else if (time_index >= dest[DATA_DEST].capacity)
	{ // ERROR: we are not fast enough to keep up with the data stream
	  d = TRASH_DEST;
	  time_index = 0;
	  nlost++;
	}
      else if (time_index < dest[TEMP_DEST].capacity)
	{ // the current heap will go into the temporary buffer until it is full and is copied at the beginning of the data buffer
	  d = TEMP_DEST;
	}
      else
	{ // the current heap will go into the ringbuffer slot
	  d = DATA_DEST;
	}
    }
  mem_base   = dest[d].ptr.ptr();
  mem_offset = time_size*time_index;
  // add the remaining offsets
  // The term "+ freq_step/2" allows to have nonintegral freq_step.
  mem_offset += feng_size*(feng_id   - feng_first              );
  mem_offset += freq_size*(frequency - freq_first + freq_step/2)/freq_step;
  heap2dest[ph->heap_cnt] = d; // store the relation between heap counter and destination
  return pointer((std::uint8_t*)(mem_base + mem_offset), deleter(shared_from_this(), (void *) std::uintptr_t(size)));
}

void ringbuffer_allocator::free(std::uint8_t *ptr, void *user)
{
}

void ringbuffer_allocator::handle_data_full()
{
  std::lock_guard<std::mutex> lock(dest_mutex);
  // release the current ringbuffer slot
  dada.data_stream().release();	
  // get a new ringbuffer slot
  dest[DATA_DEST].ptr    = dada.data_stream().next();
  dest[DATA_DEST].needed = dest[DATA_DEST].space;
  dest[DATA_DEST].first  = dest[TEMP_DEST].first;
  // switch to parallel data/temp order
  state = PARALLEL_STATE;
}

void ringbuffer_allocator::handle_temp_full()
{
  memcpy(dest[DATA_DEST].ptr.ptr(), dest[TEMP_DEST].ptr.ptr(), dest[TEMP_DEST].space*heap_size);
  std::lock_guard<std::mutex> lock(dest_mutex);
  dest[TEMP_DEST].needed  = dest[TEMP_DEST].space;
  dest[TEMP_DEST].first   = 0;
  dest[DATA_DEST].needed -= dest[TEMP_DEST].space;
  // switch to sequential data/temp order
  state = SEQUENTIAL_STATE;
}

void do_handle_data_full(ringbuffer_allocator *rb)
{
  rb->handle_data_full();
}

void do_handle_temp_full(ringbuffer_allocator *rb)
{
  rb->handle_temp_full();
}

void ringbuffer_allocator::mark(spead2::recv::heap &heap)
{
  spead2::s_item_pointer_t cnt = heap.get_cnt();
  std::size_t              nd, nt;

  {
    std::lock_guard<std::mutex> lock(dest_mutex);
    int d = heap2dest[cnt];
    dest[d].needed--;
    heap2dest.erase(cnt);
    nd = dest[DATA_DEST].needed;
    nt = dest[TEMP_DEST].needed;
  }
  if (nd == 0)
    {
      std::thread dfull(do_handle_data_full, this);
    }
  else if (nt == 0)
    {
      std::thread tfull(do_handle_temp_full, this);
    }
}

class fengine_stream : public spead2::recv::stream
{
private:
  std::int64_t                           n_complete = 0;
  const options                          opts;
  std::promise<void>                     stop_promise;
  std::shared_ptr<ringbuffer_allocator>  rbuffer;

  virtual void heap_ready(spead2::recv::live_heap &&heap) override;

public:
  template<typename... Args>
  fengine_stream(const options &opts, Args&&... args)
    : spead2::recv::stream::stream(std::forward<Args>(args)...),
      opts(opts) {}

  virtual void stop_received() override;
  std::int64_t join();
  void set_ringbuffer(std::shared_ptr<ringbuffer_allocator> rb);
};

void fengine_stream::heap_ready(spead2::recv::live_heap &&heap)
{
  if (heap.is_contiguous())
    {
      spead2::recv::heap frozen(std::move(heap));
      show_heap(frozen, opts);
      rbuffer->mark(frozen);
      n_complete++;
    }
  else
    {
      std::cout << "Discarding incomplete heap " << heap.get_cnt() << '\n';
    }
}

void fengine_stream::stop_received()
{
  spead2::recv::stream::stop_received();
  stop_promise.set_value();
}

std::int64_t fengine_stream::join()
{
  std::future<void> future = stop_promise.get_future();
  future.get();
  return n_complete;
}

void fengine_stream::set_ringbuffer(std::shared_ptr<ringbuffer_allocator> rb)
{
  rbuffer = rb;
  set_memory_allocator(rbuffer);
}



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
            stream->emplace_reader<spead2::recv::udp_reader>(endpoint, opts.packet, opts.buffer);
        }
    }
    return stream;
}

int main(int argc, const char **argv)
{
    options opts = parse_args(argc, argv);

    spead2::thread_pool thread_pool(opts.threads);
    std::vector<std::unique_ptr<fengine_stream> > streams;
    if (opts.joint)
    {
        streams.push_back(make_stream(thread_pool, opts, opts.sources.begin(), opts.sources.end()));
    }
    else
    {
        for (auto it = opts.sources.begin(); it != opts.sources.end(); ++it)
            streams.push_back(make_stream(thread_pool, opts, it, it + 1));
    }

    std::int64_t n_complete = 0;
    for (const auto &ptr : streams)
      {
	auto &stream = dynamic_cast<fengine_stream &>(*ptr);
	n_complete += stream.join();
      }

    std::cout << "Received " << n_complete << " heaps\n";
    return 0;
}
