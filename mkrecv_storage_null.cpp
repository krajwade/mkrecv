#include "mkrecv_storage_null.h"

namespace mkrecv
{

  storage_null::storage_null(std::shared_ptr<mkrecv::options> hopts) :
    storage(hopts)
  {
    std::size_t  data_size  = MAX_DATA_SPACE;
    std::size_t  trash_size = MAX_TEMPORARY_SPACE;

    if (heap_size != 0)
      {
	std::size_t group_size;
	group_size = heap_count*(heap_size + nsci*sizeof(spead2::s_item_pointer_t));
	data_size  = group_size; // 1 heap group
	trash_size = group_size; // 1 heap group
      }
    dest[DATA_DEST].allocate_buffer(memallocator, data_size);
    std::cout << "dest[DATA_DEST].ptr.ptr()  = " << (std::size_t)(dest[DATA_DEST].ptr->ptr())  << '\n';
    dest[TRASH_DEST].allocate_buffer(memallocator, trash_size);
    std::cout << "dest[TRASH_DEST].ptr.ptr() = " << (std::size_t)(dest[TRASH_DEST].ptr->ptr()) << '\n';
  }

  storage_null::~storage_null()
  {
  }

  int storage_null::handle_alloc_place(spead2::s_item_pointer_t &group_index, int dest_index)
  {
    group_index = 0;
    return dest_index;
  }
  
  void storage_null::request_stop()
  {
    stop = true;
  }

  bool storage_null::is_stopped()
  {
    return has_stopped;
  }

  void storage_null::close()
  {
  }


}
