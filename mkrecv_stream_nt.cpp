#include <iostream>
#include <utility>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string>
#include <memory>
#include <atomic>

#include "mkrecv_stream_nt.h"

namespace mkrecv
{

  typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;

  static time_point start = std::chrono::high_resolution_clock::now();

  void stream_nt::show_heap(const spead2::recv::heap &fheap)
  {
    if (opts->quiet)
      return;
    time_point now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = now - start;
    std::cout << std::showbase;
    std::cout << "Received heap " << fheap.get_cnt() << " at " << elapsed.count() << '\n';
    if (opts->descriptors)
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

  void stream_nt::heap_ready(spead2::recv::live_heap &&heap)
  {
    ha->mark(heap.get_cnt(), heap.is_contiguous(), heap.get_received_length());
    if (heap.is_contiguous())
      {
	//spead2::recv::heap frozen(std::move(heap));
	//show_heap(frozen);
	//rbuffer->mark(heap.get_cnt(), true, heap.get_received_length());
	n_complete++;
      }
    else
      {
	//std::cout << "Discarding incomplete heap " << heap.get_cnt() << '\n';
	//rbuffer->mark(heap.get_cnt(), false, heap.get_received_length());
      }
    //if (rbuffer->is_stopped()) stop();
  }

  void stream_nt::stop_received()
  {
    std::cout << "fengine::stop_received()" << '\n';
    spead2::recv::stream::stop_received();
    std::cout << "  after spead2::recv::stream::stop_received()" << '\n';
    stop_promise.set_value();
    std::cout << "  after stop_promise.set_value()" << '\n';
  }

  std::int64_t stream_nt::join()
  {
    std::cout << "stream::join()" << '\n';
    std::future<void> future = stop_promise.get_future();
    future.get();
    return n_complete;
  }

  void stream_nt::set_allocator(std::shared_ptr<mkrecv::allocator_nt> hha)
  {
    ha = hha;
    set_memory_allocator(ha);
  }


}
