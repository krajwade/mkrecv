
#include "ascii_header.h"

#include "mkrecv_fengine_allocator.h"

namespace mkrecv
{

  fengine_allocator::fengine_allocator(key_t key, std::string mlname, std::shared_ptr<options> opts) :
    allocator::allocator(key, mlname, opts)
  {
  }

  fengine_allocator::~fengine_allocator()
  {
  }


  int fengine_allocator::handle_data_heap(std::size_t size,
					  void *hint,
					  std::size_t &heap_index)
  {
    heap_index = (indices[0].index*indices[1].count + indices[1].index)*indices[2].count + indices[2].index;
    return DATA_DEST;
  }

  void fengine_allocator::mark_log()
  {
  }

}
