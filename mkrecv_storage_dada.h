#ifndef mkrecv_storage_full_dada_h
#define mkrecv_storage_full_dada_h

#include "psrdada_cpp/dada_write_client.hpp"

#include "mkrecv_storage_null.h"
#include "mkrecv_destination.h"

namespace mkrecv
{

  class storage_full_dada : public storage_null
  {
  protected:
    psrdada_cpp::MultiLog              mlog;
    psrdada_cpp::DadaWriteClient       dada;
    psrdada_cpp::RawBytes             *hdr;   // memory to store constant (header) information
    std::thread                        header_thread;
    std::thread                        switch_thread;
    std::thread                        copy_thread;
  public:
    storage_full_dada(std::shared_ptr<mkrecv::options> opts, key_t key, std::string mlname);
    int alloc_place(spead2::s_item_pointer_t timestamp,    // timestamp of a heap
		    std::size_t heap_index,                // heap number inside a heap group
		    std::size_t size,                      // heap size (only payload)
		    int dest_index,                        // requested destination (DATA_DEST _or_ TRASH_DEST)
		    char *&heap_place,                     // returned memory pointer to this heap payload
		    spead2::s_item_pointer_t *&sci_place); // returned memory pointer to the side-channel items for this heap
    void free_place(spead2::s_item_pointer_t timestamp,    // timestamp of a heap
		    int dest,                              // real destination of a heap (DATA_DEST, TEMP_DEST or TRASH_DEST)
		    std::size_t reclen);                   // recieved number of bytes
    void close();
    void proc_header();
    void proc_switch_slot();
    void proc_copy_temp();
  protected:
    void proc_init(spead2::s_item_pointer_t timestamp,     // timestamp of a heap
		   std::size_t size                        // heap size (only payload)
		   );
  };

}

#endif /* mkrecv_storage_full_dada_h */

