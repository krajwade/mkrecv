#ifndef mkrecv_storage_single_buffer_h
#define mkrecv_storage_single_buffer_h

#include "mkrecv_storage.h"
#include "mkrecv_destination.h"

namespace mkrecv
{

  class storage_single_buffer : public storage
  {
  public:
    storage_single_buffer(std::shared_ptr<mkrecv::options> opts, bool alloc_data = true, bool alloc_temp = true);
    ~storage_single_buffer();
    void request_stop();
    bool is_stopped();
    void close();
  protected:
    int handle_alloc_place(spead2::s_item_pointer_t &group_index, int dest_index);
    void handle_free_place(spead2::s_item_pointer_t timestamp, int dest_index);
    virtual void do_switch_slot();
    virtual void do_release_slot();
    virtual void do_copy_temp();
  };

}

#endif /* mkrecv_storage_single_buffer_h */

