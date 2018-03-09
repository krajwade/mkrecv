#include <cstdlib>

#include "mkrecv_destination.h"

namespace mkrecv {

  destination::destination() : mptr(), ptr(NULL)
  {
  }

  destination::~destination()
  {
    /*
    if (is_owner)
      {
	if (ptr != NULL)
	  {
	    delete ptr->ptr();
	    delete ptr;
	    ptr = NULL;
	  }
      }
      */
  }

  void destination::set_buffer(psrdada_cpp::RawBytes *ptr, std::size_t size)
  {
    this->size = size;
    this->ptr = ptr;
  }

  void destination::allocate_buffer(std::shared_ptr<spead2::mmap_allocator> memallocator, std::size_t size)
  {
    this->size = size;
    is_owner = true;
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

  void destination::set_heap_size(std::size_t heap_size, std::size_t heap_count, std::size_t nbgroups)
  {
    if (nbgroups == 0)
      {
	nbgroups = size/(heap_size*heap_count);
      }
    capacity = nbgroups;
    space = capacity*heap_count;
    needed = space;
    count = 0;
    cts = 0;
  }

}
