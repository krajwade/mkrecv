#ifndef mkrecv_fengine_stream_h
#define mkrecv_fengine_stream_h

#include <cstdint>
#include <atomic>

#include "mkrecv_options.h"
#include "mkrecv_ringbuffer_allocator.h"

namespace mkrecv
{

  class fengine_stream : public spead2::recv::stream
  {
  private:
    std::int64_t                           n_complete = 0;
    const options                          opts;
    std::promise<void>                     stop_promise;
    std::shared_ptr<ringbuffer_allocator>  rbuffer;
    
    void show_heap(const spead2::recv::heap &fheap);
    virtual void heap_ready(spead2::recv::live_heap &&heap) override;

  public:
    template<typename... Args>
      fengine_stream(const mkrecv::options &opts, Args&&... args)
      : spead2::recv::stream::stream(std::forward<Args>(args)...),
      opts(opts) {}
    
    virtual void stop_received() override;
    std::int64_t join();
    void set_ringbuffer(std::shared_ptr<ringbuffer_allocator> rb);
  };

}

#endif /* mkrecv_fengine_stream_h */
