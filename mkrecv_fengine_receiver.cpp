
#include "psrdada_cpp/cli_utils.hpp"

#include "mkrecv_fengine_receiver.h"

namespace po = boost::program_options;
namespace asio = boost::asio;

namespace mkrecv
{

  fengine_receiver::fengine_receiver()
  {
  }

  fengine_receiver::~fengine_receiver()
  {
  }

  std::shared_ptr<mkrecv::allocator> fengine_receiver::create_allocator()
  {
  }

}
