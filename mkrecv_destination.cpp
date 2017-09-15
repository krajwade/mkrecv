#include <cstdlib>

#include "mkrecv_destination.h"

namespace mkrecv {

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

}
