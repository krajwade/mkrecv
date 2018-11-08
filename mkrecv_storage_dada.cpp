#include "mkrecv_storage_dada.h"

namespace mkrecv
{

  storage_dada::storage_dada(std::shared_ptr<mkrecv::options> hopts, key_t key, std::string mlname) :
    storage_null(hopts, false),
    mlog(mlname),
    dada(key, mlog),
    hdr(NULL)
  {
    hdr = &dada.header_stream().next();
    dest[DATA_DEST].set_buffer(&dada.data_stream().next(), dada.data_buffer_size());
    header_thread = std::thread([this] ()
				{
				  this->proc_header();
				});
    switch_thread = std::thread([this] ()
				{
				  this->proc_switch_slot();
				});
    copy_thread = std::thread([this] ()
			      {
				this->proc_copy_temp();
			      });
    std::cout << "dest[DATA_DEST].ptr.ptr()  = " << (std::size_t)(dest[DATA_DEST].ptr->ptr()) << std::endl;
  }

  storage_dada::~storage_dada()
  {
  }

  void storage_dada::proc_header()
  {
    std::unique_lock<std::mutex> lck(header_mutex);
    while (header_cv.wait_for(lck, std::chrono::milliseconds(500)) == std::cv_status::timeout) {
      if (has_stopped) return;
    }
    opts->set_start_time(timestamp_first);
    memcpy(hdr->ptr(),
	   opts->header,
	   (DADA_DEFAULT_HEADER_SIZE < hdr->total_bytes()) ? DADA_DEFAULT_HEADER_SIZE : hdr->total_bytes());
    hdr->used_bytes(hdr->total_bytes());
    dada.header_stream().release();
    hdr = NULL;
  }

  void storage_dada::do_init(spead2::s_item_pointer_t timestamp,     // timestamp of a heap
			     std::size_t size                        // heap size (only payload)
			     )
  {
    storage_null::do_init(timestamp, size);
    std::unique_lock<std::mutex> lck(header_mutex);
    header_cv.notify_all();
  }

  void storage_dada::proc_switch_slot()
  {
    do {
      std::unique_lock<std::mutex> lck(switch_mutex);
      while (switch_cv.wait_for(lck, std::chrono::milliseconds(50)) == std::cv_status::timeout) {
	if (has_stopped) return;
      }
      spead2::s_item_pointer_t  *sci_base = (spead2::s_item_pointer_t*)(dest[DATA_DEST].ptr->ptr()
									+ dest[DATA_DEST].size
									- dest[DATA_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
      memcpy(sci_base, dest[DATA_DEST].sci, dest[DATA_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
      memset(dest[DATA_DEST].sci, 0, dest[DATA_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
      // Release the current slot and get a new one
      dest[DATA_DEST].ptr->used_bytes(dest[DATA_DEST].ptr->total_bytes());
      dada.data_stream().release();
      dest[DATA_DEST].ptr = &dada.data_stream().next();
    } while (true);
  }

  void storage_dada::proc_copy_temp()
  {
    do {
      std::unique_lock<std::mutex> lck(copy_mutex);
      while (copy_cv.wait_for(lck, std::chrono::milliseconds(50)) == std::cv_status::timeout) {
	if (has_stopped) return;
      }
      memcpy(dest[DATA_DEST].ptr->ptr(), dest[TEMP_DEST].ptr->ptr(), dest[TEMP_DEST].space*heap_size);
      if (nsci != 0)
	{ // copy the side-channel items in temporary space into data space and clear the source
	  memcpy(dest[DATA_DEST].sci, dest[TEMP_DEST].sci, dest[TEMP_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
	  memset(dest[TEMP_DEST].sci, 0, dest[TEMP_DEST].space*nsci*sizeof(spead2::s_item_pointer_t));
	}
    } while (true);
  }
  
  void storage_dada::do_switch_slot()
  {
    std::unique_lock<std::mutex> lck(switch_mutex);
    switch_cv.notify_all();
  }
  
  void storage_dada::do_release_slot()
  {
    // release the previously allocated slot without any data -> used as end signal
    dada.data_stream().release();
  }
  
  void storage_dada::do_copy_temp()
  {
    std::unique_lock<std::mutex> lck(copy_mutex);
    copy_cv.notify_all();
  }
  
  void storage_dada::close()
  {
    // Avoid messages when exiting the program
    if (header_thread.joinable()) header_thread.join();
    if (switch_thread.joinable()) switch_thread.join();
    if (copy_thread.joinable()) copy_thread.join();
  }
  

}
