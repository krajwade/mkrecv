#ifndef mkrecv_destination_h
#define mkrecv_destination_h

#include <cstdint>

#include "psrdada_cpp/dada_write_client.hpp"
#include "spead2/common_memory_allocator.h"

namespace mkrecv
{

  class destination
  {
  public:
    spead2::memory_allocator::pointer          mptr;
    psrdada_cpp::RawBytes    ptr;
    std::size_t              size = 0;     // destination size in bytes
    std::size_t              capacity = 0; // the number of timestamps which fits into the destination
    std::size_t              first = 0;    // the first timestamp in a heap destination
    std::size_t              space = 0;    // number of heaps which can go into this destination
    std::size_t              needed = 0;   // number of heaps needed until the destination is full
    std::size_t              count = 0;    // number of heaps assigned to this destination
    destination();
    void set_buffer(psrdada_cpp::RawBytes &ptr, std::size_t size);
    void allocate_buffer(std::shared_ptr<spead2::mmap_allocator> memallocator, std::size_t size);
  };

}

#endif /* mkrecv_destination_h */
