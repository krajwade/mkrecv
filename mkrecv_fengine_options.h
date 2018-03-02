#ifndef mkrecv_fengine_options_h
#define mkrecv_fengine_options_h

#include "mkrecv_options.h"

#define SAMPLE_CLOCK_OPT   "sample_clock"
#define SAMPLE_CLOCK_KEY   "SAMPLE_CLOCK"
#define SAMPLE_CLOCK_DESC  "virtual sample clock used for calculations"
#define SAMPLE_CLOCK_DEF   1750000000.0

#define SYNC_EPOCH_OPT     "sync_epoch"
#define SYNC_EPOCH_KEY     "SYNC_TIME"
#define SYNC_EPOCH_DESC    "the ADC sync epoch"
#define SYNC_EPOCH_DEF     0.0

#define TIME_STEP_OPT      "time_step"
#define TIME_STEP_KEY      "TIME_STEP"
#define TIME_STEP_DESC     "difference between consecutive timestamps"
#define TIME_STEP_DEF      0x200000   // 2^21

#define SAMPLE_CLOCK_START_KEY  "SAMPLE_CLOCK_START"
#define UTC_START_KEY           "UTC_START"

/* The following options influence directly the behaviour as heap filter */
#define FREQ_FIRST_OPT     "freq_first"
#define FREQ_FIRST_KEY     "FREQ_FIRST"
#define FREQ_FIRST_DESC    "lowest frequency in all incomming heaps"
#define FREQ_FIRST_DEF     0

#define FREQ_STEP_OPT      "freq_step"
#define FREQ_STEP_KEY      "FREQ_STEP"
#define FREQ_STEP_DESC     "difference between consecutive frequencies"
#define FREQ_STEP_DEF      512

#define FREQ_COUNT_OPT     "freq_count"
#define FREQ_COUNT_KEY     "FREQ_COUNT"
#define FREQ_COUNT_DESC    "number of frequency bands"
#define FREQ_COUNT_DEF     8

#define FENG_FIRST_OPT     "feng_first"
#define FENG_FIRST_KEY     "FIRST_ANT"
#define FENG_FIRST_DESC    "lowest fengine id"
#define FENG_FIRST_DEF     0

#define FENG_COUNT_OPT     "feng_count"
#define FENG_COUNT_KEY     "NANTS"
#define FENG_COUNT_DESC    "number of fengines"
#define FENG_COUNT_DEF     0

namespace po = boost::program_options;

namespace mkrecv
{

  class fengine_options : public mkrecv::options
  {
  public:
    double                    sample_clock    = SAMPLE_CLOCK_DEF;
    double                    sync_epoch      = SYNC_EPOCH_DEF;
    std::size_t               time_step       = TIME_STEP_DEF;
    // heap filter mechanism
    std::size_t               freq_first      = FREQ_FIRST_DEF;
    std::size_t               freq_step       = FREQ_STEP_DEF;
    std::size_t               freq_count      = FREQ_COUNT_DEF;
    std::size_t               feng_first      = FENG_FIRST_DEF;
    std::size_t               feng_count      = FENG_COUNT_DEF;
  private:

  public:
    fengine_options();
    ~fengine_options();
    void set_start_time(int64_t timestamp);
    void create_args();
    void apply_header();
  };

}

#endif /* mkrecv_fengine_options_h */
