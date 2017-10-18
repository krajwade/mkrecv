#include <iostream>
#include <utility>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string>
#include <memory>
#include <atomic>

#include "mkrecv_fengine_stream.h"

namespace mkrecv
{

  typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;

  static time_point start = std::chrono::high_resolution_clock::now();

  void fengine_stream::show_heap(const spead2::recv::heap &fheap)
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

  void fengine_stream::heap_ready(spead2::recv::live_heap &&heap)
  {
    rbuffer->mark(heap.get_cnt(), heap.is_contiguous(), heap.get_received_length());
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


}
