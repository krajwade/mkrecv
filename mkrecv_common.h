#ifndef mkrecv_common_h
#define mkrecv_common_h

#define ENABLE_TIMING_MEASUREMENTS    1
//#define USE_STD_MUTEX                 1
//#define ENABLE_CORE_MUTEX  1

namespace mkrecv {

  template<class T>
    void inc(T &value, const T max_value)
    {
      if (value == max_value)
        {
          value = 0;
        }
      else
        {
          value++;
        }
    }

  template<class T>
    void dec(T &value, const T max_value)
    {
      if (value == 0)
        {
          value = max_value;
        }
      else
        {
          value--;
        };
    }

}

#endif /* mkrecv_common_h */

