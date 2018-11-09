#ifndef mkrecv_storage_single_dada_h
#define mkrecv_storage_single_dada_h

#include <mutex>
#include <condition_variable>

#include "psrdada_cpp/dada_write_client.hpp"

#include "mkrecv_storage_single_buffer.h"
#include "mkrecv_destination.h"

namespace mkrecv
{

  class storage_single_dada : public storage_single_buffer
  {
  protected:
    psrdada_cpp::MultiLog              mlog;
    psrdada_cpp::DadaWriteClient       dada;
    psrdada_cpp::RawBytes             *hdr;   // memory to store constant (header) information
    std::mutex                         header_mutex;
    std::condition_variable            header_cv;
    std::thread                        header_thread;
    std::mutex                         switch_mutex;
    std::condition_variable            switch_cv;
    std::thread                        switch_thread;
    std::mutex                         copy_mutex;
    std::condition_variable            copy_cv;
    std::thread                        copy_thread;
  public:
    storage_single_dada(std::shared_ptr<mkrecv::options> opts, key_t key, std::string mlname);
    ~storage_single_dada();
    void close();
    void proc_header();
    void proc_switch_slot();
    void proc_copy_temp();
  protected:
    void do_init(spead2::s_item_pointer_t timestamp,     // timestamp of a heap
		 std::size_t size                        // heap size (only payload)
		 );
    void do_switch_slot();
    void do_release_slot();
    void do_copy_temp();
  };

}

#endif /* mkrecv_storage_single_dada_h */

