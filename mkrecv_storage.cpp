#include "mkrecv_storage.h"

namespace mkrecv
{

  storage::storage(std::shared_ptr<mkrecv::options> hopts) :
    opts(hopts)
#ifndef USE_STD_MUTEX
    , dest_sem(1)
#endif
  {
    int i;
    
    heap_size = opts->heap_size;
    heap_count = 1;
    for (i = 0; i < opts->nindices; i++)
      {
	int count = opts->indices[i].values.size();
	if (count == 0) count = 1;
	heap_count *= count;
      }
    timestamp_step = opts->indices[0].step;
    nsci = opts->nsci;
    scis = opts->scis;
    memallocator = std::make_shared<spead2::mmap_allocator>(0, true);
  }

  storage::~storage()
  {
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::cout << "storage: ";
    et.show();
#endif
  }

}
