#ifndef mkrecv_receiver_h
#define mkrecv_receiver_h

#include <vector>

#include <boost/asio.hpp>

#include <spead2/common_thread_pool.h>
#include <spead2/common_defines.h>

#include "mkrecv_options.h"
#include "mkrecv_destination.h"
#include "mkrecv_allocator.h"
#include "mkrecv_stream.h"

namespace po = boost::program_options;
namespace asio = boost::asio;

namespace mkrecv
{

  class receiver
  {
  protected:
    static receiver *instance;
  protected:
    std::shared_ptr<mkrecv::options>                opts = NULL;
    std::shared_ptr<mkrecv::allocator>              allocator = NULL;
    std::vector<std::unique_ptr<mkrecv::stream> >   streams;
    std::shared_ptr<spead2::thread_pool>            thread_pool = NULL;

  public:
    receiver();
    virtual ~receiver();
    int execute(int argc, const char **argv);
    virtual std::shared_ptr<mkrecv::options> create_options();
    virtual std::shared_ptr<mkrecv::allocator> create_allocator();
    virtual std::unique_ptr<mkrecv::stream> create_stream();
  public:
    static void request_stop();
  protected:
    std::unique_ptr<mkrecv::stream> make_stream(std::vector<std::string>::iterator first_source,
						std::vector<std::string>::iterator last_source);
  };

}

#endif /* mkrecv_receiver_h */
