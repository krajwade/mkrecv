#include <cstdlib>

#include "mkrecv_destination.h"

namespace mkrecv {

destination::destination() : mptr(), ptr(NULL) {}

void destination::set_buffer(psrdada_cpp::RawBytes *ptr, std::size_t size)
{
  this->size = size;
  this->ptr = ptr;
}

void destination::allocate_buffer(std::shared_ptr<spead2::mmap_allocator> memallocator, std::size_t size)
{
  this->size = size;
  if (memallocator == NULL)
    {
      this->ptr  = new psrdada_cpp::RawBytes(new char[size], size, 0);
    }
  else
    {
      mptr = memallocator->allocate(size, NULL);
      this->ptr = new psrdada_cpp::RawBytes((char*)mptr.get(), size, 0);
    }
}

}
