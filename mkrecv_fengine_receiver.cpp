
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

  std::shared_ptr<mkrecv::options> fengine_receiver::create_options()
  {
    fopts = std::shared_ptr<mkrecv::fengine_options>(new fengine_options());
    return fopts;
  }

  std::shared_ptr<mkrecv::allocator> fengine_receiver::create_allocator()
  {
    fallocator = std::shared_ptr<mkrecv::fengine_allocator>(new fengine_allocator(psrdada_cpp::string_to_key(opts->key), "recv", fopts));
    return fallocator;
  }

  std::unique_ptr<mkrecv::stream> fengine_receiver::create_stream()
  {
    spead2::bug_compat_mask bug_compat = opts->pyspead ? spead2::BUG_COMPAT_PYSPEAD_0_5_2 : 0;

    return std::unique_ptr<mkrecv::stream>(new mkrecv::stream(opts, thread_pool, bug_compat, opts->heaps));
  }

}
