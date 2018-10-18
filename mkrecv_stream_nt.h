#ifndef mkrecv_stream_nt_h
#define mkrecv_stream_nt_h

#include <cstdint>
#include <atomic>

#include "mkrecv_options.h"
#include "mkrecv_allocator_nt.h"

namespace mkrecv
{

  class stream_nt : public spead2::recv::stream
  {
  private:
    std::int64_t                            n_complete = 0;
    const std::shared_ptr<mkrecv::options>  opts = NULL;
    std::promise<void>                      stop_promise;
    std::shared_ptr<mkrecv::allocator_nt>   ha = NULL;
    
    void show_heap(const spead2::recv::heap &fheap);
    virtual void heap_ready(spead2::recv::live_heap &&heap) override;

  public:
    template<typename... Args>
      stream_nt(const std::shared_ptr<mkrecv::options> opts, Args&&... args)
      : spead2::recv::stream::stream(std::forward<Args>(args)...),
      opts(opts) {}
    
    void stop_received(); // override;
    std::int64_t join();
    void set_allocator(std::shared_ptr<mkrecv::allocator_nt> ha);
  };

}

#endif /* mkrecv_stream_nt_h */
