#ifndef mkrecv_storage_null_h
#define mkrecv_storage_null_h

#include "mkrecv_storage.h"
#include "mkrecv_destination.h"

namespace mkrecv
{

  class storage_null : public storage
  {
  public:
    storage_null(std::shared_ptr<mkrecv::options> opts);
    ~storage_null();
    void request_stop();
    bool is_stopped();
    void close();
  protected:
    int handle_alloc_place(spead2::s_item_pointer_t &group_index, int dest_index);
  };

}

#endif /* mkrecv_storage_null_h */

