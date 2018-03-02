

#include <string.h>

#include <iostream>     // std::cout
#include <fstream>      // std::ifstream

#include "ascii_header.h"

#include "mkrecv_fengine_options.h"


namespace mkrecv
{

  /*
  template<typename T>
  static po::typed_value<T> *make_opt(T &var)
  {
    return po::value<T>(&var)->default_value(var);
  }
  */

  template<typename T>
  static po::typed_value<T> *make_opt(T &var)
  {
    return po::value<T>(&var);
  }

  /*
  static po::typed_value<bool> *make_opt(bool &var)
  {
    return po::bool_switch(&var)->default_value(var);
  }
  */

  static po::typed_value<bool> *make_opt(bool &var)
  {
    return po::bool_switch(&var);
  }
  
  fengine_options::fengine_options()
  {
  }

  fengine_options::~fengine_options()
  {
  }

  void fengine_options::set_start_time(int64_t timestamp)
  {
    double epoch = sync_epoch + timestamp / sample_clock;
    double integral;
    double fractional = modf(epoch, &integral);
    struct timeval tv;
    time_t start_utc;
    struct tm *nowtm;
    char tbuf[64];
    char utc_string[64];

    tv.tv_sec = integral;
    tv.tv_usec = (int) (fractional*1e6);
    start_utc = tv.tv_sec;
    nowtm = gmtime(&start_utc);
    strftime(tbuf, sizeof(tbuf), DADA_TIMESTR, nowtm);
    snprintf(utc_string, sizeof(utc_string), "%s.%06ld", tbuf, tv.tv_usec);
    ascii_header_set(header, SAMPLE_CLOCK_START_KEY, "%ld", timestamp);
    if (!check_header())
      {
	std::cerr << "ERROR, storing " << SAMPLE_CLOCK_START_KEY << " with value " << timestamp << " in header failed due to size restrictions. -> incomplete header due to clipping" << std::endl;
      }
    ascii_header_set(header, UTC_START_KEY, "%s", utc_string);
    if (!check_header())
      {
	std::cerr << "ERROR, storing " << UTC_START_KEY << " with value " << utc_string << " in header failed due to size restrictions. -> incomplete header due to clipping" << std::endl;
      }
  }

  void fengine_options::create_args()
  {
    options::create_args();
    desc.add_options()
      (SAMPLE_CLOCK_OPT, make_opt(sample_clock),    SAMPLE_CLOCK_DESC)
      (SYNC_EPOCH_OPT,   make_opt(sync_epoch),      SYNC_EPOCH_DESC)
      (TIME_STEP_OPT,    make_opt(time_step),       TIME_STEP_DESC)
      // heap filter mechanism
      (FREQ_FIRST_OPT,   make_opt(freq_first),      FREQ_FIRST_DESC)
      (FREQ_STEP_OPT,    make_opt(freq_step),       FREQ_STEP_DESC)
      (FREQ_COUNT_OPT,   make_opt(freq_count),      FREQ_COUNT_DESC)
      (FENG_FIRST_OPT,   make_opt(feng_first),      FENG_FIRST_DESC)
      (FENG_COUNT_OPT,   make_opt(feng_count),      FENG_COUNT_DESC)
      ;
  }

  /*
   * Each parameter can be specified as default value, program option and header value. If a command line option is given,
   * it takes precedence over a header value which takes precedence over the default value.
   * Only program parameters which have an influence on the behaviour can have a header key value and are stored in the
   * header copied into the ringbuffer.
   */
  void fengine_options::apply_header()
  {
    options::apply_header();
    set_opt(sample_clock,    SAMPLE_CLOCK_OPT, SAMPLE_CLOCK_KEY);
    set_opt(sync_epoch,      SYNC_EPOCH_OPT, SYNC_EPOCH_KEY);
    set_opt(time_step,       TIME_STEP_OPT,  TIME_STEP_KEY);
    /* The following options influence directly the behaviour as heap filter */
    set_opt(freq_first,      FREQ_FIRST_OPT, FREQ_FIRST_KEY);
    set_opt(freq_step,       FREQ_STEP_OPT,  FREQ_STEP_KEY);
    set_opt(freq_count,      FREQ_COUNT_OPT, FREQ_COUNT_KEY);
    set_opt(feng_first,      FENG_FIRST_OPT, FENG_FIRST_KEY);
    set_opt(feng_count,      FENG_COUNT_OPT, FENG_COUNT_KEY);
  }

}
