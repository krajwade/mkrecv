#ifndef mkrecv_destination_h
#define mkrecv_destination_h

#include <cstdint>

#include "psrdada_cpp/dada_write_client.hpp"
#include <spead2/common_defines.h>
#include "spead2/common_memory_allocator.h"

namespace mkrecv
{

  class destination
  {
  private:
    bool    is_owner = false;
  public:
    spead2::memory_allocator::pointer          mptr = NULL;
    psrdada_cpp::RawBytes    *ptr = NULL;
    std::size_t               size = 0;     // destination size in bytes
    std::size_t               capacity = 0; // number of groups which can go into this destination
    std::size_t               space = 0;    // number of heaps which can go into this destination
    std::size_t               needed = 0;   // number of heaps needed until the destination is full
    std::size_t               count = 0;    // number of heaps assigned to this destination
    std::size_t               cts = 0;      // number of completed heaps before switching from sequential to parallel
    spead2::s_item_pointer_t *sci = NULL;   // memory for storing the sid-channel items which are copied afterwards into a slot
    destination();
    ~destination();
    void set_buffer(psrdada_cpp::RawBytes *ptr, std::size_t size);
    void allocate_buffer(std::shared_ptr<spead2::mmap_allocator> memallocator, std::size_t size);
    void set_heap_size(std::size_t heap_size, std::size_t heap_count, std::size_t nbgroups = 0, std::size_t nbsci = 0);
  };

}

#endif /* mkrecv_destination_h */
