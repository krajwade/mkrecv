#ifndef mkrecv_storage_null_h
#define mkrecv_storage_null_h

#include "mkrecv_storage.h"
#include "mkrecv_destination.h"

namespace mkrecv
{

  class storage_null : public storage
  {
  protected:
    std::mutex                         dest_mutex;
    destination                        dest[3];
    std::size_t                        log_counter = 0;
  public:
    storage_null(std::shared_ptr<mkrecv::options> opts, bool alloc_data = true);
    ~storage_null();
    int alloc_place(spead2::s_item_pointer_t timestamp,    // timestamp of a heap
		    std::size_t heap_index,                // heap number inside a heap group
		    std::size_t size,                      // heap size (only payload)
		    int dest_index,                        // requested destination (DATA_DEST _or_ TRASH_DEST)
		    char *&heap_place,                     // returned memory pointer to this heap payload
		    spead2::s_item_pointer_t *&sci_place); // returned memory pointer to the side-channel items for this heap
    void free_place(spead2::s_item_pointer_t timestamp,    // timestamp of a heap
		    int dest,                              // real destination of a heap (DATA_DEST, TEMP_DEST or TRASH_DEST)
		    std::size_t reclen);                   // recieved number of bytes
    void request_stop();
    bool is_stopped();
    void close();
  protected:
    virtual void do_init(spead2::s_item_pointer_t timestamp,     // timestamp of a heap
			 std::size_t size                        // heap size (only payload)
			 );
    virtual void do_switch_slot();
    virtual void do_release_slot();
    virtual void do_copy_temp();
    void show_mark_log();
    void show_state_log();
  };

}

#endif /* mkrecv_storage_null_h */

