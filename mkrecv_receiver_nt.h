#ifndef mkrecv_receiver_nt_h
#define mkrecv_receiver_nt_h

#include <vector>
#include <thread>

#include <boost/asio.hpp>

#include <spead2/common_thread_pool.h>
#include <spead2/common_defines.h>

#include "mkrecv_options.h"
#include "mkrecv_destination.h"
#include "mkrecv_allocator_nt.h"
#include "mkrecv_storage.h"
#include "mkrecv_stream_nt.h"

namespace po = boost::program_options;
namespace asio = boost::asio;

namespace mkrecv
{

  class receiver_nt
  {
  protected:
    static receiver_nt     *instance;
  protected:
    std::shared_ptr<mkrecv::options>                      opts = NULL;
    std::shared_ptr<mkrecv::storage>                      storage = NULL;
    std::vector<std::shared_ptr<mkrecv::allocator_nt> >   allocators;
    std::vector<std::unique_ptr<mkrecv::stream_nt> >      streams;
    std::shared_ptr<spead2::thread_pool>                  thread_pool = NULL;

  public:
    receiver_nt();
    virtual ~receiver_nt();
    int execute(int argc, const char **argv);
    std::shared_ptr<mkrecv::options> create_options();
    std::shared_ptr<mkrecv::storage> create_storage();
    std::shared_ptr<mkrecv::allocator_nt> create_allocator();
    std::unique_ptr<mkrecv::stream_nt> create_stream();
  public:
    static void request_stop();
  protected:
    std::unique_ptr<mkrecv::stream_nt> make_stream(std::vector<std::string>::iterator first_source,
						   std::vector<std::string>::iterator last_source);
  };

}

#endif /* mkrecv_receiver_nt_h */
