#ifndef mkrecv_fengine_receiver_h
#define mkrecv_fengine_receiver_h


#include "mkrecv_receiver.h"
#include "mkrecv_fengine_options.h"
#include "mkrecv_fengine_allocator.h"

namespace mkrecv
{

  class fengine_receiver : public mkrecv::receiver
  {
  protected:
    std::shared_ptr<mkrecv::fengine_allocator>  fallocator;

  public:
    fengine_receiver();
    virtual ~fengine_receiver();
    virtual std::shared_ptr<mkrecv::allocator> create_allocator();
  };

}

#endif /* mkrecv_fengine_receiver_h */
