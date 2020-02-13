
#include <string.h>
#include <sys/mman.h>

#include <iostream>     // std::cout
#include <fstream>      // std::ifstream

#define BOOST_LOG_DYN_LINK 1

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

#include "ascii_header.h"

#include "mkrecv_rnt.h"

namespace mkrecv
{

  // *******************************************************************
  // ***** class storage_statistics
  // *******************************************************************

  storage_statistics::storage_statistics()
  {
    heaps_total     = 0;
    heaps_completed = 0;
    heaps_discarded = 0;
    heaps_skipped   = 0;
    heaps_overrun   = 0;
    heaps_ignored   = 0;
    heaps_too_old   = 0;
    heaps_present   = 0;
    heaps_too_new   = 0;
    heaps_open      = 0;
    heaps_needed    = 0;
    bytes_expected  = 0;
    bytes_received  = 0;
  }

  void storage_statistics::reset()
  {
    heaps_total     = 0;
    heaps_completed = 0;
    heaps_discarded = 0;
    heaps_skipped   = 0;
    heaps_overrun   = 0;
    heaps_ignored   = 0;
    heaps_too_old   = 0;
    heaps_present   = 0;
    heaps_too_new   = 0;
    heaps_open      = 0;
    heaps_needed    = 0;
    bytes_expected  = 0;
    bytes_received  = 0;
  }

  // *******************************************************************
  // ***** class timing_statistics
  // *******************************************************************

#ifdef ENABLE_TIMING_MEASUREMENTS
  timing_statistics::timing_statistics()
  {
    std::int64_t i;
    
    for (i = 0; i < 2; i++) {
      min_et[i] = 1.0e30;
      max_et[i] = 0.0;
      sum_et[i] = 0.0;
      count_et[i] = 0.0;
    }
    for (i = 0; i < MAX_SLOTS; i++) {
      histo[0][i] = 0;
      histo[1][i] = 0;
    }
  }

  void timing_statistics::add_et(std::int64_t fi, double nanoseconds)
  {
    if (nanoseconds < min_et[fi]) min_et[fi] = nanoseconds;
    if (nanoseconds > max_et[fi]) max_et[fi] = nanoseconds;
    sum_et[fi]   += nanoseconds;
    count_et[fi] += 1.0;
    if (nanoseconds <= 10.0) {
      histo[fi][0]++;
    } else if (nanoseconds <= 100.0) {
      if (nanoseconds <= 20.0) {
        histo[fi][1]++;
      } else if (nanoseconds <= 50.0) {
        histo[fi][2]++;
      } else {
        histo[fi][3]++;
      }
    } else if (nanoseconds <= 1000.0) {
      if (nanoseconds <= 200.0) {
        histo[fi][4]++;
      } else if (nanoseconds <= 500.0) {
        histo[fi][5]++;
      } else {
        histo[fi][6]++;
      }
    } else if (nanoseconds <= 10000.0) {
      if (nanoseconds <= 2000.0) {
        histo[fi][7]++;
      } else if (nanoseconds <= 5000.0) {
        histo[fi][8]++;
      } else {
        histo[fi][9]++;
      }
    } else if (nanoseconds <= 100000.0) {
      if (nanoseconds <= 20000.0) {
        histo[fi][10]++;
      } else if (nanoseconds <= 50000.0) {
        histo[fi][11]++;
      } else {
        histo[fi][12]++;
      }
    } else if (nanoseconds <= 1000000.0) {
      if (nanoseconds <= 200000.0) {
        histo[fi][13]++;
      } else if (nanoseconds <= 500000.0) {
        histo[fi][14]++;
      } else {
        histo[fi][15]++;
      }
    } else if (nanoseconds <= 10000000.0) {
      if (nanoseconds <= 2000000.0) {
        histo[fi][16]++;
      } else if (nanoseconds <= 5000000.0) {
        histo[fi][17]++;
      } else {
        histo[fi][18]++;
      }
    } else if (nanoseconds <= 100000000.0) {
      if (nanoseconds <= 20000000.0) {
        histo[fi][19]++;
      } else if (nanoseconds <= 50000000.0) {
        histo[fi][20]++;
      } else {
        histo[fi][21]++;
      }
    } else if (nanoseconds <= 1000000000.0) {
      if (nanoseconds <= 200000000.0) {
        histo[fi][22]++;
      } else if (nanoseconds <= 500000000.0) {
        histo[fi][23]++;
      } else {
        histo[fi][24]++;
      }
    } else {
      histo[fi][LARGER_SLOT]++;
    }
  }
  
  void timing_statistics::show()
  {
    std::cout << "et:"
              << " alloc " << sum_et[ALLOC_TIMING]/count_et[ALLOC_TIMING] << " [" << min_et[ALLOC_TIMING] << " .. " << max_et[ALLOC_TIMING] << "]"
              << " mark " << sum_et[MARK_TIMING]/count_et[MARK_TIMING] << " [" << min_et[MARK_TIMING] << " .. " << max_et[MARK_TIMING] << "]"
              << '\n';
    std::cout << "   10 ns "  << histo[ALLOC_TIMING][0]  << " " << histo[MARK_TIMING][0]  << '\n';
    std::cout << "   20 ns "  << histo[ALLOC_TIMING][1]  << " " << histo[MARK_TIMING][1]  << '\n';
    std::cout << "   50 ns "  << histo[ALLOC_TIMING][2]  << " " << histo[MARK_TIMING][2]  << '\n';
    std::cout << "   100 ns " << histo[ALLOC_TIMING][3]  << " " << histo[MARK_TIMING][3]  << '\n';
    std::cout << "   200 ns " << histo[ALLOC_TIMING][4]  << " " << histo[MARK_TIMING][4]  << '\n';
    std::cout << "   500 ns " << histo[ALLOC_TIMING][5]  << " " << histo[MARK_TIMING][5]  << '\n';
    std::cout << "   1 us "   << histo[ALLOC_TIMING][6]  << " " << histo[MARK_TIMING][6]  << '\n';
    std::cout << "   2 us "   << histo[ALLOC_TIMING][7]  << " " << histo[MARK_TIMING][7]  << '\n';
    std::cout << "   5 us "   << histo[ALLOC_TIMING][8]  << " " << histo[MARK_TIMING][8]  << '\n';
    std::cout << "   10 us "  << histo[ALLOC_TIMING][9]  << " " << histo[MARK_TIMING][9]  << '\n';
    std::cout << "   20 us "  << histo[ALLOC_TIMING][10] << " " << histo[MARK_TIMING][10] << '\n';
    std::cout << "   50 us "  << histo[ALLOC_TIMING][11] << " " << histo[MARK_TIMING][11] << '\n';
    std::cout << "   100 us " << histo[ALLOC_TIMING][12] << " " << histo[MARK_TIMING][12] << '\n';
    std::cout << "   200 us " << histo[ALLOC_TIMING][13] << " " << histo[MARK_TIMING][13] << '\n';
    std::cout << "   500 us " << histo[ALLOC_TIMING][14] << " " << histo[MARK_TIMING][14] << '\n';
    std::cout << "   1 ms "   << histo[ALLOC_TIMING][15] << " " << histo[MARK_TIMING][15] << '\n';
    std::cout << "   2 ms "   << histo[ALLOC_TIMING][16] << " " << histo[MARK_TIMING][16] << '\n';
    std::cout << "   5 ms "   << histo[ALLOC_TIMING][17] << " " << histo[MARK_TIMING][17] << '\n';
    std::cout << "   10 ms "  << histo[ALLOC_TIMING][18] << " " << histo[MARK_TIMING][18] << '\n';
    std::cout << "   20 ms "  << histo[ALLOC_TIMING][19] << " " << histo[MARK_TIMING][19] << '\n';
    std::cout << "   50 ms "  << histo[ALLOC_TIMING][20] << " " << histo[MARK_TIMING][20] << '\n';
    std::cout << "   100 ms " << histo[ALLOC_TIMING][21] << " " << histo[MARK_TIMING][21] << '\n';
    std::cout << "   200 ms " << histo[ALLOC_TIMING][22] << " " << histo[MARK_TIMING][22] << '\n';
    std::cout << "   500 ms " << histo[ALLOC_TIMING][23] << " " << histo[MARK_TIMING][23] << '\n';
  }

  void timing_statistics::reset()
  {
    std::int64_t i;
    
    for (i = 0; i < 2; i++) {
      min_et[i] = 1.0e30;
      max_et[i] = 0.0;
      sum_et[i] = 0.0;
      count_et[i] = 0.0;
    }
    for (i = 0; i < MAX_SLOTS; i++) {
      histo[0][i] = 0;
      histo[1][i] = 0;
    }
  }
#endif

  // *******************************************************************
  // ***** class index
  // *******************************************************************

  index::index()
  {
  }

  index::~index()
  {
    if (map != NULL) {
      delete map;
      map = NULL;
    }
  }

  void index::update_mapping(std::int64_t index_size)
  {
    std::int64_t              i;
    spead2::s_item_pointer_t  val;
    std::int64_t              valsize  = 0;
    spead2::s_item_pointer_t  valmask  = 0;

    item  *= sizeof(spead2::item_pointer_t);
    count  = values.size();
    std::cout << "count " << count << " list items\n";
    if (count == 0) count = 1;
    for (i = 0; i < (std::int64_t)values.size(); i++) {
      spead2::s_item_pointer_t k = values[i]; // key   := element in the option list (--idx<i>-list)
      std::int64_t             v = i*index_size;  // value := heap index [B] calculated from list position and index_size
      if ((i == 0) || (k < valmin)) valmin = k;
      if ((i == 0) || (k > valmax)) valmax = k;
      valbits |= k;
      value2index[k] = v;
      std::cout << "  hmap " << k << " -> " << v << '\n';
    }
    if (valbits != 0) {
      val = valbits;
      while ((val & 1) == 0) {
        valshift++;
        val = val >> 1;
      }
      mapmax = val;
      while (val != 0) {
        valsize++;
        val = val >> 1;
      }
      valmask = (1 << valsize) - 1;
    }
    std::cout << "  valbits " << valbits << " size " << valsize << " shift " << valshift << " valmask " << valmask << " valmin " << valmin << " valmax " << valmax << " mapmax " << mapmax << '\n';
    if (mapmax < 4096) {
      // we use a direct approach for mapping pointer item values into indices
      map = new std::int64_t[mapmax + 1];
      for (val = 0; val <= mapmax; val++) {
        map[val] = (std::int64_t)-1;
      }
      for (i = 0; i < (std::int64_t)values.size(); i++) {
        spead2::s_item_pointer_t k   = values[i]; // key   := element in the option list (--idx<i>-list)
        std::int64_t              v   = i*index_size;  // value := heap index [B] calculated from list position and index_size
        spead2::s_item_pointer_t idx = (k >> valshift);  // index in direct map (k shifted by valshift in order to remove the 0-bits)
        map[idx] = v;
        std::cout << "  dmap " << k << " -> " << idx << " -> " << v << '\n';
      }
    }
  }

  std::int64_t index::v2i(spead2::s_item_pointer_t v, bool uhm)
  {
    if (uhm || (map == NULL)) {
      try {
        return value2index.at(v);
      }
      catch (const std::out_of_range& oor) {
        return (std::int64_t)-1;
      }
    } else if (v & ~valbits) {
      return -1;
    } else if (v < valmin) {
      return -1;
    } else if (v > valmax) {
      return -1;
    } else {
      spead2::s_item_pointer_t hv = v >> valshift;
      if (hv > mapmax) return -1;
      return map[hv];
    }
  }

  // *******************************************************************
  // ***** class options
  // *******************************************************************

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

  options::options()
  {
    size_t i;

    header = new char[DADA_DEFAULT_HEADER_SIZE + DADA_DEFAULT_HEADER_SIZE];
    for (i = 0; i < sizeof(header); i++) header[i] = '\0';
    header[DADA_DEFAULT_HEADER_SIZE+1] = ASCII_HEADER_SENTINEL;
  }

  options::~options()
  {
    delete header;
  }

  void options::usage(std::ostream &o)
  {
    if (!ready)
      {
        ready = true;
        create_args();
      }
    o << "Usage: mkrecv [options] { Multicast-IP }\n";
    o << desc;
  }
  
  void options::parse_args(int argc, const char **argv)
  {
    if (!ready) {
      ready = true;
      create_args();
    }
    all.add(desc);
    all.add(hidden);
    try {
      po::store(po::command_line_parser(argc, argv)
                .style(po::command_line_style::default_style & ~po::command_line_style::allow_guessing)
                .options(all)
                .positional(positional)
                .run(), vm);
      po::notify(vm);
      if (vm.count("help")) {
        usage(std::cout);
        std::exit(0);
      }
      /*
        if (!vm.count("source"))
        throw po::error("At least one IP is required");
      */
      if (vm.count("source")) {
        sources_opt = vm["source"].as<std::vector<std::string>>();
        if (sources_opt.size() != 0) {
          bool is_first = true;
          for (std::vector<std::string>::iterator ipi = sources_opt.begin(); ipi != sources_opt.end(); ++ipi) {
            if (!is_first) {
              sources_str += ',';
            }
            sources_str += *ipi;
            is_first = false;
          }
        }
      }
      //if (hdrname != "")
      if (vm.count(HEADER_OPT) != 0) {
        // read the given template header file and adapt some configuration parameters
        //hdrname = vm[HEADER_OPT].as<std::string>();
        //std::cout << "try to load file " << hdrname << '\n';
        load_header();
      }
      apply_header(); // use values from header or default values
      return;
    }
    catch (po::error &e) {
      std::cerr << e.what() << '\n';
      usage(std::cerr);
      std::exit(2);
    }
  }

  void options::load_header()
  {
    std::ifstream   is(hdrname, std::ifstream::binary);

    if (is) {
      // get length of file:
      is.seekg (0, is.end);
      uint64_t length = is.tellg();
      is.seekg (0, is.beg);
      if (length > DADA_DEFAULT_HEADER_SIZE) {
        std::cerr << "options::load_header(), given file " << hdrname << " contains more than " << DADA_DEFAULT_HEADER_SIZE << " characters, ognoring this file." << '\n';
        return;
      }
      //std::cout << "options::load_header(), loading file " << hdrname << '\n';
      is.read(header, length);
      if (!is) {
        std::cerr << "error: only " << is.gcount() << " could be read" << '\n';
      }
      is.close();
    }
  }

  void options::set_start_time(int64_t timestamp)
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
    if (!check_header()) {
      std::cerr << "ERROR, storing " << SAMPLE_CLOCK_START_KEY << " with value " << timestamp << " in header failed due to size restrictions. -> incomplete header due to clipping" << '\n';
    }
    ascii_header_set(header, UTC_START_KEY, "%s", utc_string);
    if (!check_header()) {
      std::cerr << "ERROR, storing " << UTC_START_KEY << " with value " << utc_string << " in header failed due to size restrictions. -> incomplete header due to clipping" << '\n';
    }
  }

  /*
    Parse an option given as default (string value), header file entry or program option and converts it into
    an integer number. It is allowed to use decimals, hexadecimals (prefix 0x) or binary (prefix 0b) notation.
    We have to distinguish three cases:
    1. The parameter is specified as program option, this value does overwrite the default value and the value given in the header file.
    2. The parameter is set in the header file but not as a program option, this value does overwrite the default value and it is stored in the header file (unchanged).
    3. Only the default value is given, this value is stored in the header file (unchanged).
  */
  USED_TYPE options::finalize_parameter(std::string &val, const char *opt, const char *key)
  {
    USED_TYPE ut;
    if (!quiet) std::cout << "finalize_parameter(" << val << ", " << opt << ", " << key << ")";
    if (vm.count(opt) != 0) { // Check if the parameter is given as program option
      // -> use the program option value already given in val and store it in the header file
      ut = OPTION_USED;
      if (!quiet) std::cout << " option for " << opt << " is " << val;
      if (key[0] != '\0') {
        if (val.length() == 0) {
          ascii_header_set(header, key, "unset");
          if (!quiet) std::cout << "  header becomes unset" << '\n';
        } else {
          ascii_header_set(header, key, "%s", val.c_str());
          if (!quiet) std::cout << "  header becomes " << val << '\n';
        }
        if (!check_header()) {
          std::cerr << "ERROR, storing " << key << " with value " << val << " in header failed due to size restrictions. -> incomplete header due to clipping" << '\n';
        }
      }
    } else if (key[0] != '\0') { // Check if a value is given in the header file
      char sval[1024];
      if (ascii_header_get(header, key, "%s", sval) != -1) {
        if (strcmp(sval, "unset") == 0) { // check if the value in the header is unset
          // -> use the default value already given in val
          ut = DEFAULT_USED;
          if (!quiet) std::cout << " header unset, default for " << opt << " is " << val;
        } else {
          // -> use the value from the header file
          ut = CONFIG_USED;
          val = sval;
          if (!quiet) std::cout << " header for " << opt << " is " << val;
        }
      } else {
        ut = DEFAULT_USED;
        if (!quiet) std::cout << " default for " << opt << " is " << val;
      }
    }
    if (!quiet) std::cout << " -> " << val << '\n';
    return ut;
  }

  bool options::parse_fixnum(std::int64_t &val, std::string &val_str)
  {
    try {
      if (val_str.compare(0,2,"0x") == 0) {
        val = std::stol(std::string(val_str.begin() + 2, val_str.end()), nullptr, 16);
      } else if (val_str.compare(0,2,"0b") == 0) {
        val = std::stol(std::string(val_str.begin() + 2, val_str.end()), nullptr, 2);
      } else {
        val = std::stol(val_str, nullptr, 10);
      }
      return true;
    } catch (std::exception& e) {
      return false;
    }
  }

  USED_TYPE options::parse_parameter(std::string &val, const char *opt, const char *key)
  {
    USED_TYPE ut = finalize_parameter(val, opt, key);
    //std::cout << opt << "/" << key << " = " << val << '\n';
    return ut;
  }

  USED_TYPE options::parse_parameter(std::int64_t &val, std::string &val_str, const char *opt, const char *key)
  {
    USED_TYPE ut = finalize_parameter(val_str, opt, key);
    // we have the final value in val_str -> convert it into an integer number according to an optional prefix
    if (!parse_fixnum(val, val_str)) {
      std::cerr << "Exception: cannot convert " << val_str << " into int for option " << opt << "/" << key << '\n';
      // put the default value already given in val into the header (the current value cannot be converted into std::int64_t)
      if (key[0] != '\0') {
        ascii_header_set(header, key, "%ld", val);
        //std::cout << "  header becomes " << val << '\n';
        if (!check_header()) {
          std::cerr << "ERROR, storing " << key << " with value " << val << " in header failed due to size restrictions. -> incomplete header due to clipping" << '\n';
        }
      }
    }
    if (!quiet) std::cout << opt << "/" << key << " = " << val << '\n';
    return ut;
  }

  USED_TYPE options::parse_parameter(double &val, std::string &val_str, const char *opt, const char *key)
  {
    USED_TYPE ut = finalize_parameter(val_str, opt, key);
    // we have the final value in val_str -> convert it into an integer number according to an optional prefix
    try {
      val = std::stod(val_str, nullptr);
    } catch (std::exception& e) {
      std::cerr << "Exception: " << e.what() << " cannot convert " << val_str << " into double for option " << opt << "/" << key << '\n';
      // put the default value already given in val into the header (the current value cannot be converted into std::int64_t)
      if (key[0] != '\0') {
        ascii_header_set(header, key, "%f", val);
        //std::cout << "  header becomes " << val << '\n';
        if (!check_header()) {
          std::cerr << "ERROR, storing " << key << " with value " << val << " in header failed due to size restrictions. -> incomplete header due to clipping" << '\n';
        }
      }
    }
    if (!quiet) std::cout << opt << "/" << key << " = " << val << '\n';
    return ut;
  }

  /*
    Parse a list of integer values where each value is either a direct value or a range definition.
    All integer values (direct or in a range definition) are either a decimal, hexadecimal or binary number.
    The syntax for an integer list is:
    <list> := "unset" | ( <element> { "," <element> } ) .
    <element> := <number> | ( <first> ":" <last> ":" <step> ) .
    <number>  := <decimal> | <hexadecimal> | <binary> .
  */

  USED_TYPE options::parse_parameter(std::vector<std::int64_t> &val, std::string &val_str, const char *opt, const char *key)
  {
    std::string::size_type str_from = 0, str_to, str_length;

    USED_TYPE ut = finalize_parameter(val_str, opt, key);
    // we have the final value in val_str -> convert it into an list of integer number according to an optional prefix and with range support
    val.clear();
    str_length = val_str.length();
    while(str_from < str_length + 1) {
      str_to = val_str.find_first_of(",", str_from);
      if(str_to == std::string::npos) str_to = str_length;
      if(str_to == str_from) break;
      std::string               el_str(val_str.data() + str_from, str_to - str_from);
      std::string::size_type    el_from = 0, el_to, el_length;
      std::int64_t              vals[3];
      std::int64_t              nparts = 0;
      el_length = el_str.length();
      while((el_from < el_length + 1) && (nparts != 3)) {
        el_to = el_str.find_first_of(":", el_from);
        if(el_to == std::string::npos) el_to = el_length;
        if(el_to == el_from) break;
        std::string hel(el_str.data() + el_from, el_to - el_from);
        if (!parse_fixnum(vals[nparts], hel)) {
          std::cerr << "Exception: cannot convert " << hel << " at position " << str_from << " into std::int64_t for option " << opt << "/" << key << '\n';
          nparts = 0;
          break;
        }
        nparts++;
        el_from = el_to + 1;
      }
      if (nparts < 3) vals[2] = 1;           // <first> ':' <last>  => <step> := 1
      if (nparts < 2) vals[1] = vals[0] + 1; // <first>             => <step> := 1, <last> := <first> + 1
      if (!quiet) std::cout << "  sequence from " << vals[0] << " to " << vals[1] << " (excluding) with step " << vals[2] << '\n';
      while (vals[0] < vals[1]) {
        val.push_back(vals[0]);
        vals[0] += vals[2];
      }
      str_from = str_to + 1;
    }
    if (!quiet) {
      std::cout << "item value list:";
      std::size_t i;
      for (i = 0; i < val.size(); i++) {
        std::cout << " " << val.at(i) << "->" << i;
      }
      std::cout << '\n';
    }
    return ut;
  }

  /*
    two kinds of notation:
    1. <a0> "." <a1> "." <a2> "." <a3> [ "+" <offset> [ "|" <step> ] ] [ ":" <port> ] .
    2. <a0> "." <a1> "." <a2> "." <a3> [ "+" <offset> [ ":" <step> ] ] .
  */
  USED_TYPE options::parse_parameter(std::vector<std::string> &val, std::string &val_str, const char *opt, const char *key)
  {
    std::string::size_type str_from = 0, str_to, str_length;

    USED_TYPE ut = finalize_parameter(val_str, opt, key);
    // we have the final value in val_str -> convert it into an list of IPs allowing "i.j.k.l+n" notation
    val.clear();
    str_length = val_str.length();
    while(str_from < str_length + 1) {
      str_to = val_str.find_first_of(",", str_from);
      if(str_to == std::string::npos) str_to = str_length;
      if(str_to == str_from) break;
      std::string               el_str(val_str.data() + str_from, str_to - str_from);
      std::string::size_type    el_from = 0, el_to, el_length;
      std::int64_t              vals[8];
      std::int64_t              nadr = 0;
      std::int64_t              ipart;
      char                      pch = '.'; // prefix char: '.' -> address byte, '+' -> offset, '|' -> step, ':' -> port
      bool                      isok = true;
#ifdef COMBINED_IP_PORT
      bool                      has_port = false;
#endif
      for (ipart = 0; ipart < 8; ipart++) vals[ipart] = 0;
      el_length = el_str.length();
      while((el_from < el_length + 1)) {
        el_to = el_str.find_first_of(".+|:", el_from);
        if(el_to == std::string::npos) el_to = el_length;
        if(el_to == el_from) break;
        std::string hel(el_str.data() + el_from, el_to - el_from);
#ifdef COMBINED_IP_PORT
        // 0 .. 3  = address part
        // 4       = offset
        // 5       = step;
        // 6       = port
        // 7       = trash/unknown
        switch (pch) {
        case '.':
          if (nadr < 4) {
            ipart = nadr++;
          } else {
            ipart = 7; // -> trash
          }
          break;
        case '+':
          ipart = 4;
          break;
        case '|':
          ipart = 5;
          break;
        case ':':
          ipart = 6;
          has_port = true;
          break;
        default:
          ipart = 7; // -> trash
        }
#else
        // 0 .. 3  = address part
        // 4       = offset
        // 5       = step;
        // 7       = trash/unknown
        switch (pch) {
        case '.':
          if (nadr < 4) {
            ipart = nadr++;
          } else {
            ipart = 7; // -> trash
          }
          break;
        case '+':
          ipart = 4;
          break;
        case ':':
          ipart = 5;
          break;
        default:
          ipart = 7; // -> trash
        }
#endif
        if (!parse_fixnum(vals[ipart], hel)) {
          std::cerr << "Exception: cannot convert " << hel << " at position " << str_from << " into int for option " << opt << "/" << key << '\n';
          isok = false;
          break;
        }
        if (el_to < el_length) pch = el_str.at(el_to);
        el_from = el_to + 1;
      }
      str_from = str_to + 1;
      if (!isok) continue;
      if (nadr != 4) {
        std::cerr << "Not a valid IPv4 address, less than 4 address bytes: " << el_str << '\n';
        continue;
      }
      if (vals[5] == 0) vals[5] = 1;
#ifdef COMBINED_IP_PORT
      if (!quiet) std::cout << "  IP sequence for " << vals[0] << "." << vals[1] << "." << vals[2] << "." << vals[3] << "+" << vals[4] << "|" << vals[5] << ":" << vals[6] << '\n';
#else
      if (!quiet) std::cout << "  IP sequence for " << vals[0] << "." << vals[1] << "." << vals[2] << "." << vals[3] << "+" << vals[4] << ":" << vals[5] << '\n';
#endif
      vals[4] += 1; // the n means up to l+n _including_
      while (vals[4] > 0) {
        std::string ip_str;
        char ip_adr[256];
        snprintf(ip_adr, sizeof(ip_adr), "%ld.%ld.%ld.%ld", vals[0], vals[1], vals[2], vals[3]);
        ip_str = ip_adr;
#ifdef COMBINED_IP_PORT
        ip_str += ":";
        if (has_port) {
          char ip_port[16];
          snprintf(ip_port, sizeof(ip_adr), "%ld", vals[6]);
          ip_str += ip_port;
        } else {
          ip_str += port;
        }
#endif
        val.push_back(ip_str);
        vals[3] += vals[5];
        vals[4] -= vals[5];
      }
    }
    if (!quiet) {
      std::cout << "item value list:";
      std::size_t i;
      for (i = 0; i < val.size(); i++)
        {
          std::cout << " " << val.at(i) << "->" << i;
        }
      std::cout << '\n';
    }
    return ut;
  }

  bool options::check_index_specification()
  {
    std::int64_t  i;
    bool          result = true;

    // check 1. index specification (timestamp)
    if (nindices == 0) {
      std::cerr << "ERROR: at least the first index (timestamp/serial number) must be specified\n";
      result = false;
    }
    if (indices[0].item_used_type == DEFAULT_USED) {
      std::cerr << "ERROR: please specify the item pointer which contains the timestamp/serial number (IDX1_ITEM)\n";
      result = false;
    }
    if (indices[0].step_used_type == DEFAULT_USED) {
      std::cerr << "ERROR: please specify the difference between successivee timestamp/serial numbers (IDX1_STEP)\n";
      result = false;
    }
    for (i = 1; i < nindices; i++) {
      if (indices[i].item_used_type == DEFAULT_USED) {
        std::cerr << "ERROR: please specify the item pointer which contains the index value (IDX" << (i+1) << "_ITEM)\n";
        result = false;
      }
      if (indices[i].list_used_type == DEFAULT_USED) {
        std::cerr << "ERROR: please specify the item pointer values (IDX" << (i+1) << "_LIST)\n";
        result = false;
      }
    }
    return result;
  }

  bool options::check_header()
  {
    bool result = (header[DADA_DEFAULT_HEADER_SIZE+1] == ASCII_HEADER_SENTINEL);
    header[DADA_DEFAULT_HEADER_SIZE] = '\0';
    header[DADA_DEFAULT_HEADER_SIZE+1] = ASCII_HEADER_SENTINEL;
    return result;
  }

  void options::create_args()
  {
    std::int64_t i;

    desc.add_options()
      ("help",            "show this text")
      // optional header file contain configuration options and additional information
      (HEADER_OPT,        make_opt(hdrname),             "name of a template header file which may include configuration options")
      // some flags
      (QUIET_OPT,         make_opt(quiet),               "Only show total of heaps received")
      (DESCRIPTORS_OPT,   make_opt(descriptors),         "Show descriptors")
      (JOINT_OPT,         make_opt(joint),               "Treat all sources as a single stream")
      (JOINT_OPT,         make_opt(joint),               "Treat all sources as a single stream")
      (DNSWTWR_OPT,       make_opt(dnswtwr),             "Do not stop when timestamp will rollover")
      // some options, default values should be ok to use, will _not_ go into header
      (PACKET_NBYTES_OPT, make_opt(packet_nbytes_str),   "Maximum packet size to use for UDP")
      (BUFFER_NBYTES_OPT, make_opt(buffer_nbytes_str),   "Socket buffer size")
      (NTHREADS_OPT,      make_opt(nthreads_str),        "Number of worker threads")
      (NHEAPS_OPT,        make_opt(nheaps_str),          "Maximum number of in-flight heaps")
      ("memcpy-nt",       make_opt(memcpy_nt),           "Use non-temporal memcpy")
      // DADA ringbuffer related stuff
      (DADA_KEY_OPT,      make_opt(dada_key),            "PSRDADA ring buffer key")
      (DADA_NSLOTS_OPT,   make_opt(dada_nslots_str),     "Maximum number of open dada ringbuffer slots")
      // network configuration
#if SPEAD2_USE_IBV
      (IBV_IF_OPT,        make_opt(ibv_if),              "Interface address for ibverbs")
      (IBV_VECTOR_OPT,    make_opt(ibv_comp_vector_str), "Interrupt vector (-1 for polled)")
      (IBV_MAX_POLL_OPT,  make_opt(ibv_max_poll_str),    "Maximum number of times to poll in a row")
#endif
      (UDP_IF_OPT,        make_opt(udp_if),              "UDP interface")
      (PORT_OPT,          make_opt(port),                "Port number")
      (SYNC_EPOCH_OPT,    make_opt(sync_epoch_str),      "the ADC sync epoch")
      (SAMPLE_CLOCK_OPT,  make_opt(sample_clock_str),    "virtual sample clock used for calculations")
      (HEAP_NBYTES_OPT,   make_opt(heap_nbytes_str),     "The heap size used for checking incomming heaps.")
      (LEVEL_DATA_OPT,    make_opt(level_data_str),      "Fill level of the data destination before switching to sequential mode (percent).")
      ;
    // index calculation
    desc.add_options()
      (NINDICES_OPT,     make_opt(nindices_str),         "Number of item pointers used as index")
      ;
    for (i = 0; i < MAX_INDICES; i++) {
      char olabel[32];
      char odesc[255];
      snprintf(olabel, sizeof(olabel) - 1, IDX_ITEM_OPT,  i+1);
      snprintf(odesc,  sizeof(odesc)  - 1, IDX_ITEM_DESC, i+1);
      desc.add_options()(olabel, make_opt(indices[i].item_str), odesc);
      if (i == 0) {
        desc.add_options()(IDX_STEP_OPT, make_opt(indices[i].step_str), IDX_STEP_DESC);
        desc.add_options()(IDX_MOD_OPT,  make_opt(indices[i].mod_str),  IDX_MOD_DESC);
      } else {
        snprintf(olabel, sizeof(olabel) - 1, IDX_MASK_OPT,  i+1);
        snprintf(odesc,  sizeof(odesc)  - 1, IDX_MASK_DESC, i+1);
        desc.add_options()(olabel, make_opt(indices[i].mask_str), odesc);
        snprintf(olabel, sizeof(olabel) - 1, IDX_LIST_OPT,  i+1);
        snprintf(odesc,  sizeof(odesc)  - 1, IDX_LIST_DESC, i+1);
        desc.add_options()(olabel, make_opt(indices[i].list), odesc);
      }
    }
    desc.add_options()
      (SCI_LIST_OPT,  make_opt(sci_list), "list of item pointers going into a side-channel inside a ringbuffer slot")
      ;
    hidden.add_options()
      // network configuration
      (SOURCES_OPT,      po::value<std::vector<std::string>>()->composing(), "sources");
    positional.add("source", -1);
  }

  /*
   * Each parameter can be specified as default value, program option and header value. If a command line option is given,
   * it takes precedence over a header value which takes precedence over the default value.
   * Only program parameters which have an influence on the behaviour can have a header key value and are stored in the
   * header copied into the ringbuffer.
   */
  void options::apply_header()
  {
    std::int64_t           i;

    /* the following options should have sufficient default values */
    parse_parameter(nthreads,        nthreads_str,        NTHREADS_OPT, NTHREADS_KEY);
    parse_parameter(nheaps,          nheaps_str,          NHEAPS_OPT, NHEAPS_KEY);
    /* The following options describe the DADA ringbuffer use */
    parse_parameter(dada_key,                             DADA_KEY_OPT, DADA_KEY_KEY);
    parse_parameter(dada_nslots,     dada_nslots_str,     DADA_NSLOTS_OPT, DADA_NSLOTS_KEY);
    /* The following options describe the connection to the F-Engines (network) */
#if SPEAD2_USE_IBV
    parse_parameter(ibv_if,                               IBV_IF_OPT, IBV_IF_KEY);
    parse_parameter(ibv_comp_vector, ibv_comp_vector_str, IBV_VECTOR_OPT, IBV_VECTOR_KEY);
    parse_parameter(ibv_max_poll,    ibv_max_poll_str,    IBV_MAX_POLL_OPT, IBV_MAX_POLL_KEY);
#endif
    parse_parameter(udp_if,                               UDP_IF_OPT, UDP_IF_KEY);
    parse_parameter(port,                                 PORT_OPT, PORT_KEY);
    parse_parameter(sample_clock,    sample_clock_str,    SAMPLE_CLOCK_OPT, SAMPLE_CLOCK_KEY);
    parse_parameter(sync_epoch,      sync_epoch_str,      SYNC_EPOCH_OPT, SYNC_EPOCH_KEY);
    parse_parameter(level_data,      level_data_str,      LEVEL_DATA_OPT, LEVEL_DATA_KEY);
    parse_parameter(nindices,        nindices_str,        NINDICES_OPT, NINDICES_KEY);
    if (nindices >= MAX_INDICES) nindices = MAX_INDICES-1;
    for (i = 0; i < nindices; i++) {
      char iopt[32];
      char ikey[32];
      snprintf(iopt, sizeof(iopt) - 1, IDX_ITEM_OPT, i+1);
      snprintf(ikey, sizeof(ikey) - 1, IDX_ITEM_KEY, i+1);
      indices[i].item_used_type = parse_parameter(indices[i].item, indices[i].item_str, iopt, ikey);
      if (i == 0) {
        indices[i].step_used_type = parse_parameter(indices[i].step, indices[i].step_str, IDX_STEP_OPT, IDX_STEP_KEY);
        indices[i].mod_used_type  = parse_parameter(indices[i].mod,  indices[i].mod_str,  IDX_MOD_OPT,  IDX_MOD_KEY);
      } else {
        snprintf(iopt, sizeof(iopt) - 1, IDX_MASK_OPT, i+1);
        snprintf(ikey, sizeof(ikey) - 1, IDX_MASK_KEY, i+1);
        indices[i].mask_used_type = parse_parameter(indices[i].mask, indices[i].mask_str, iopt, ikey);
        snprintf(iopt, sizeof(iopt) - 1, IDX_LIST_OPT, i+1);
        snprintf(ikey, sizeof(ikey) - 1, IDX_LIST_KEY, i+1);
        indices[i].list_used_type = parse_parameter(indices[i].values, indices[i].list, iopt, ikey);
      }
    }
    parse_parameter(scis, sci_list, SCI_LIST_OPT, SCI_LIST_KEY);
    nsci = scis.size();
    parse_parameter(sources, sources_str, SOURCES_OPT, SOURCES_KEY);
    // parse sizes and calculate derived values
    parse_parameter(packet_nbytes,   packet_nbytes_str,   PACKET_NBYTES_OPT, PACKET_NBYTES_KEY);
    parse_parameter(buffer_nbytes,   buffer_nbytes_str,   BUFFER_NBYTES_OPT, BUFFER_NBYTES_KEY);
    parse_parameter(heap_nbytes,     heap_nbytes_str,     HEAP_NBYTES_OPT,   HEAP_NBYTES_KEY);
    group_nheaps = 1;
    for (i = nindices - 1; i >= 0; i--) {
      indices[i].update_mapping(group_nheaps);
      group_nheaps *= indices[i].count;
    }
    group_nbytes = heap_nbytes*group_nheaps;
    if (!check_index_specification()) {
      std::cerr << "ABORTING: please change your program options and/or the configuration file\n";
      exit(-1);
    }
  }

  std::int64_t options::p2i(spead2::recv::packet_header *ph, std::size_t size, spead2::s_item_pointer_t &timestamp)
  {
    spead2::recv::pointer_decoder   decoder(ph->heap_address_bits);
    std::int64_t                    i, heap_index = 0;

    // Some heaps are ignored:
    //   If the heap is is 1
    //   If the heap size is wrong (needs a value for HEAP_SIZE of --heap-size)
    //   If there are not enough item pointers do calculate heap_index
    if (ph->heap_cnt == 1)                      
      return -1;
    if (heap_nbytes != (std::int64_t)size)                    
      return -1;
    if ((std::int64_t)(ph->n_items) < nindices) 
      return -1;
    // Extract all item pointer values and transform them into a heap index (inside a heap group)
    for (i = 0; i < nindices; i++) {
      spead2::item_pointer_t pts = spead2::load_be<spead2::item_pointer_t>(ph->pointers + indices[i].item);
      if (i == 0) {
        timestamp = decoder.get_immediate(pts);
	// check if a rollover is imminent
	if (!timestamp_rollover && (timestamp > 0xffff00000000l)) timestamp_rollover = true;
	if (timestamp_rollover) {
	  // we have a possible rollover
	  if (timestamp < 0x0000ffffffffl) {
	    // a rollover happened for this timestamp only -> add 2^48
	    if (!dnswtwr) {
	      throw std::runtime_error("A timestap rollover has happened.");
	    }
	    timestamp += (1l << 48);
	  } else if (timestamp < 0x800000000000l) {
	    // the rollover took place -> update offset and clear flag
	    timestamp_offset += (1l << 48);
	    timestamp_rollover = false;
	  }
	}
	timestamp += timestamp_offset;
      } else {
        std::int64_t item_index = indices[i].v2i((std::int64_t)(decoder.get_immediate(pts) & indices[i].mask));
        if (item_index == (std::int64_t)-1) 
	  return -1;
        heap_index += item_index;
      }
    }
    return heap_index;
  }

  // *******************************************************************
  // ***** class storage
  // *******************************************************************

  storage::storage(std::shared_ptr<mkrecv::options> hopts) :
#ifndef USE_STD_MUTEX
    dest_sem(1),
#endif
    opts(hopts)
  {
    std::stringstream   converter;
    converter << std::hex << opts->dada_key;
    converter >> key;
    heap_nbytes  = opts->heap_nbytes;
    group_nbytes = opts->group_nbytes;
    group_nheaps = opts->group_nheaps;
    nsci = opts->nsci;
    timestamp_step = opts->indices[0].step;
    timestamp_mod  = (opts->indices[0].mod/timestamp_step)*timestamp_step; // enforce that timestamp_mod is a multiple of timestamp_step
    if (timestamp_mod == 0) timestamp_mod = timestamp_step;
    header_thread = std::thread([this] ()
                                {
                                  this->create_header();
                                });
    switch_thread = std::thread([this] ()
                                {
                                  this->move_ringbuffer();
                                });
  }

  storage::~storage()
  {
  }
  
  // The alloc_data() method returns the base address to a heap group. Any further calculation
  // (address for a heap inside a heap group) must be done inside the calling class. This should
  // save time in a guarded pice of code.
  //   IN     timestamp  := SPEAD timestamp from heap
  //   IN     explen     := expected number of bytes (payload size)
  //   IN     dest_type  := destination type (TRASH_DEST or DATA_DEST)
  //   OUT    hg_payload := start address for heaps of a heap group defined by timestamp
  //   OUT    hg_sci     := start address for side channel items of a heap group defined by timestamp
  //   RETURN dest_index := (real) destination index: [0 .. nbuffers-2] -> DATA, nbuffers-1 -> TRASH
  std::int64_t storage::alloc_data(spead2::s_item_pointer_t timestamp,
                                   std::int64_t explen,
                                   char *&hg_payload,
                                   spead2::s_item_pointer_t *&hg_sci)
  {
    std::int64_t dest_index, group_index;
    
    // **** GUARDED BY SEMAPHORE ****
#ifdef USE_STD_MUTEX
    std::lock_guard<std::mutex> lock(dest_mutex);
#else
    dest_sem.get();
#endif
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
    gstat.heaps_total++;
    gstat.heaps_open++;
    gstat.bytes_expected += explen;
    if (timestamp_first == 0) {
      // it is the first heap which should go into data -> initialize
      // ensure that at least 2 timesteps are skipped
      timestamp_first = ((timestamp + timestamp_step + timestamp_mod) / timestamp_mod) * timestamp_mod;
      timestamp_level = timestamp_first + timestamp_step*timestamp_offset;
      std::cout << "sizes: buffer " << nbuffers << " " << slot_nbytes << " " << slot_ngroups
                << " heap " << heap_nbytes << " " << group_nheaps
                << " timestamp " << timestamp_first << " " << timestamp_step << " " << timestamp_level
                << '\n';
      { // start thread which allocates, fills and releases the header
        std::unique_lock<std::mutex> lck(header_mutex);
        header_cv.notify_all();
      }
    }
    if (timestamp < timestamp_first) {
      // this timestamp is too old -> trash
      dest_index  = nbuffers;
      group_index = 0;
      gstat.heaps_too_old++;
    } else {
      dest_index  = ((timestamp - timestamp_first) / timestamp_step) / slot_ngroups;
      group_index = ((timestamp - timestamp_first) / timestamp_step) % slot_ngroups;
      if (dest_index >= buffer_active) {
        // this timestamp is too far in the future -> trash
        dest_index  = nbuffers;
        group_index = 0;
        gstat.heaps_too_new++;
      } else {
        // correct the artificial destination/slot index with buffer_first
        dest_index = (dest_index + buffer_first)%nbuffers;
        gstat.heaps_present++;
      }
    }
    bstat[dest_index].heaps_total++;
    bstat[dest_index].heaps_open++;
    bstat[dest_index].bytes_expected += explen;
    hg_payload = payload_base[dest_index] + group_nbytes*group_index;
    hg_sci     = sci_base[dest_index]     +         nsci*group_index;
#ifdef ENABLE_TIMING_MEASUREMENTS
    if (gstat.heaps_total == 100*slot_nheaps) {
      et.reset();
    }
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    et.add_et(timing_statistics::ALLOC_TIMING, std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count());
#endif
#ifndef USE_STD_MUTEX
    dest_sem.put();
#endif
    return dest_index;
  }
  
  // The alloc_trash() method returns the base address to a heap group. Any further calculation
  // (address for a heap inside a heap group) must be done inside the calling class. This should
  // save time in a guarded pice of code.
  //   IN     timestamp  := SPEAD timestamp from heap
  //   IN     explen     := expected number of bytes (payload size)
  //   IN     dest_type  := destination type (TRASH_DEST or DATA_DEST)
  //   OUT    hg_payload := start address for heaps of a heap group defined by timestamp
  //   OUT    hg_sci     := start address for side channel items of a heap group defined by timestamp
  //   RETURN dest_index := (real) destination index: [0 .. nbuffers-2] -> DATA, nbuffers-1 -> TRASH
  std::int64_t storage::alloc_trash(spead2::s_item_pointer_t timestamp,
                                    std::int64_t explen,
                                    char *&hg_payload,
                                    spead2::s_item_pointer_t *&hg_sci)
  {
    (void)timestamp;
    std::int64_t dest_index, group_index;
    
    // **** GUARDED BY SEMAPHORE ****
#ifdef USE_STD_MUTEX
    std::lock_guard<std::mutex> lock(dest_mutex);
#else
    dest_sem.get();
#endif
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
    gstat.heaps_total++;
    gstat.heaps_open++;
    gstat.bytes_expected += explen;
    dest_index  = nbuffers;
    group_index = 0;
    gstat.heaps_ignored++;
    bstat[dest_index].heaps_total++;
    bstat[dest_index].heaps_open++;
    bstat[dest_index].bytes_expected += explen;
    hg_payload = payload_base[dest_index] + group_nbytes*group_index;
    hg_sci     = sci_base[dest_index]     +         nsci*group_index;
#ifdef ENABLE_TIMING_MEASUREMENTS
    if (gstat.heaps_total == 100*slot_nheaps) {
      et.reset();
    }
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    et.add_et(timing_statistics::ALLOC_TIMING, std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count());
#endif
#ifndef USE_STD_MUTEX
    dest_sem.put();
#endif
    return dest_index;
  }
  
  // The free_place() method finalize the recieving of a heap and reports the recieved number of bytes (may be less than explen).
  //   IN timestamp  := SPEAD timestamp from heap
  //   IN dest_index := destination index, from previous alloc_place() call
  //   IN reclen     := recieved number of bytes
  void storage::free_place(spead2::s_item_pointer_t timestamp, std::int64_t dest_index, std::int64_t reclen)
  {
    // **** GUARDED BY SEMAPHORE ****
#ifdef USE_STD_MUTEX
    std::lock_guard<std::mutex> lock(dest_mutex);
#else
    dest_sem.get();
#endif
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
    gstat.heaps_open--;
    gstat.bytes_received += reclen;
    bstat[dest_index].heaps_open--;
    bstat[dest_index].bytes_received += reclen;
    if (reclen == heap_nbytes) {
      gstat.heaps_completed++;
      bstat[dest_index].heaps_completed++;
    } else {
      gstat.heaps_discarded++;
      bstat[dest_index].heaps_discarded++;
    }
    if (timestamp != -1) { // -1 -> no valid heap, went into trash and is not used to release a slot
      if (timestamp > timestamp_last) timestamp_last = timestamp;
      if ((timestamp >= timestamp_level) && !switch_triggered) {
        switch_triggered = true;
        if (!has_stopped) {
          // copy the optional side-channel items at the correct position
          // sci_base = buffer + size - (scape *nsci)
          { // start thread which releases the first active slot (buffer_first), allocates a new slot and updates timestamp_first, buffer_first, statistics and pointers
            std::unique_lock<std::mutex> lck(switch_mutex);
            switch_cv.notify_all();
          }
        }
        if (stop && !has_stopped) {
          has_stopped = true;
          std::cout << "request to stop the transfer into the ringbuffer received." << '\n';
          if (ipcio_update_block_write (dada->data_block, 0) < 0) {
            multilog(mlog, LOG_ERR, "close_buffer: ipcio_update_block_write failed\n");
            throw std::runtime_error("Could not close ipcio data block");
          }
        }
      }
    }
    if ((gstat.heaps_total - log_counter) >= LOG_FREQ) {
      log_counter += LOG_FREQ;
      if (!opts->quiet) {
        std::cout << "heaps:";
        std::cout << " total "     << gstat.heaps_total     << " " << bstat[nbuffers].heaps_total;
        std::cout << " completed " << gstat.heaps_completed << " " << bstat[nbuffers].heaps_completed;
        std::cout << " discarded " << gstat.heaps_discarded << " " << bstat[nbuffers].heaps_discarded;
        std::cout << " open "      << gstat.heaps_open      << " " << bstat[nbuffers].heaps_open;
        std::cout << " age "       << gstat.heaps_too_old   << " " << gstat.heaps_present << " " << gstat.heaps_too_new;
        std::cout << " payload "   << gstat.bytes_expected  << " " << gstat.bytes_received;
        std::cout << '\n';
      }
    }
#ifndef USE_STD_MUTEX
    dest_sem.put();
#endif
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    et.add_et(timing_statistics::MARK_TIMING, std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count());
#endif
  }
  
  // Request stopping, filling the latest slot (the last currently open slot!) and closing the dada ringbuffer.
  void storage::request_stop()
  {
    stop = true;
  }
  
  // Test if storage is closed (some time after request_stop() call).
  bool storage::is_stopped()
  {
    return has_stopped;
  }
  
  // Attaching to the DADA ringbuffer, open a header and allocate a number of open slots
  void storage::open()
  {
    std::int64_t i;
    
    // opening a connection to DADA
    mlog = multilog_open("recv", 0);
    dada = dada_hdu_create(mlog);
    dada_hdu_set_key(dada, key);
    if(dada_hdu_connect(dada) < 0) {
      multilog(mlog, LOG_ERR, "Error connecting to DADA ringbuffer %s", opts->dada_key.c_str());
      throw std::runtime_error("Error connecting to DADA ringbuffer");
    }
    nslots   = ipcbuf_get_nbufs((ipcbuf_t *)dada->data_block);
    slot_nbytes  = ipcbuf_get_bufsz((ipcbuf_t *)dada->data_block);
    slot_ngroups = slot_nbytes/((heap_nbytes + nsci*sizeof(spead2::s_item_pointer_t))*group_nheaps);
    slot_nheaps  = slot_ngroups*group_nheaps;
    if (dada_hdu_lock_write (dada) < 0) {
      multilog(mlog, LOG_ERR, "Error cannot lock write access to DADA ringbuffer %s", opts->dada_key.c_str());
      throw std::runtime_error("Error cannot lock write access to DADA ringbuffer");
    }
    // allocate a certain number of open ringbuffer slots and an additional space for thrash
    nbuffers = opts->dada_nslots;
    if (nbuffers > dada->data_block->bufs_opened_max) nbuffers = dada->data_block->bufs_opened_max;
    indices       = new std::uint64_t[nbuffers+1];             // indices of all open slots
    buffers       = new char*[nbuffers+1];                     // base address for all slots and trash
    payload_base  = new char*[nbuffers+1];                     // calculated base address for the heap payload (equal with buffers)
    sci_base      = new spead2::s_item_pointer_t*[nbuffers+1]; // calculated base address for the side channel items inside a slot
    bstat         = new storage_statistics[nbuffers+1];
    buffer_active = 0;
    for (i = 0; i < nbuffers; i++) { // buffer index [0 .. nbuffers-1]
      buffers[i] = ipcio_open_block_write(dada->data_block, &(indices[i]));
      multilog(mlog, LOG_INFO, "got slot %d at %lx", indices[i], (void*)buffers[i]);
      payload_base[i] = buffers[i];
      sci_base[i]  = (spead2::s_item_pointer_t*)(buffers[i] + slot_nbytes - sizeof(spead2::s_item_pointer_t)*nsci*slot_nheaps);
      buffer_active++;
    }
    // prepare the trash memory using mmap()
    trash_nbytes = group_nbytes + sizeof(spead2::s_item_pointer_t)*nsci*group_nheaps; // include side channel items for one heap group
    trash_base = (char*)mmap(NULL,
                             trash_nbytes,
                             PROT_READ | PROT_WRITE,
                             MAP_ANONYMOUS | MAP_PRIVATE,
                             -1,
                             0);
    buffers[nbuffers]   = trash_base;
    payload_base[nbuffers] = buffers[nbuffers];
    sci_base[nbuffers]  = (spead2::s_item_pointer_t*)(buffers[nbuffers] + trash_nbytes - sizeof(spead2::s_item_pointer_t)*nsci*group_nheaps);
    timestamp_offset = (nbuffers-1)*slot_ngroups + (slot_ngroups*opts->level_data)/100;
  }
  
  // Close the dada ringbuffer access by releasing an empty slot.
  void storage::close()
  {
    if (dada_hdu_unlock_write(dada) < 0) {
      throw std::runtime_error("Error releasing HDU");
    }
    if (dada_hdu_disconnect(dada) < 0) {
      throw std::runtime_error("Unable to disconnect from hdu\n");
    }
    dada_hdu_destroy(dada);
    dada = NULL;
    multilog_close(mlog);
    mlog = NULL;
    delete indices;
    indices = NULL;
    delete buffers;
    buffers = NULL;
    delete payload_base;
    payload_base = NULL;
    delete sci_base;
    sci_base = NULL;
    delete bstat;
    bstat = NULL;
  }
  
  void storage::create_header()
  {
    std::unique_lock<std::mutex> lck(header_mutex);
    while (header_cv.wait_for(lck, std::chrono::milliseconds(500)) == std::cv_status::timeout) {
      if (has_stopped) return;
    }
    opts->set_start_time(timestamp_first);
    char* tmp = ipcbuf_get_next_write(dada->header_block);
    memcpy(tmp, opts->header, 4096); // (DADA_DEFAULT_HEADER_SIZE < hdr->total_bytes()) ? DADA_DEFAULT_HEADER_SIZE : hdr->total_bytes());
    ipcbuf_mark_filled(dada->header_block, 4096);
  }
  
  void storage::move_ringbuffer()
  {
    do {
      std::int64_t  replace_slot;
      bool          lflag;

      // wait for the switch-the-slot trigger
      std::unique_lock<std::mutex> lck(switch_mutex);
      while (switch_cv.wait_for(lck, std::chrono::milliseconds(50)) == std::cv_status::timeout) {
        if (has_stopped) return;
      }
      do {
	// remove the last slot from the internal buffers (deactivating its use, by modifying buffer_first, buffer_active and timestamp_first)
	{ // **** MUST BE GUARDED BY SEMAPHORE/MUTEX !!! ****
#ifdef USE_STD_MUTEX
	  std::lock_guard<std::mutex> lock(dest_mutex);
#else
	  dest_sem.get();
#endif
	  replace_slot = buffer_first;
	  buffer_first = (buffer_first + 1) % nbuffers;
	  buffer_active--;
	  timestamp_first += timestamp_step*slot_ngroups;
#ifndef USE_STD_MUTEX
	  dest_sem.put();
#endif
	}
	// Now we have time to release a slot
	if ((ipcio_close_block_write (dada->data_block, slot_nbytes) < 0) || (ipcbuf_mark_filled(dada->header_block, slot_nbytes) < 0))
	  {
	    multilog(mlog, LOG_ERR, "close_buffer: ipcio_update_block_write failed\n");
	    throw std::runtime_error("Could not close ipcio data block");
	  }
	// write statistics and clears it for the oldest slot
	std::cout << "STAT "
		  << slot_nheaps << " "
		  << "slot "
		  << bstat[replace_slot].heaps_completed << " " << bstat[replace_slot].heaps_discarded << " "
		  << bstat[replace_slot].heaps_open << " "
		  << bstat[replace_slot].bytes_expected << " " << bstat[replace_slot].bytes_received << " "
		  << "total "
		  << gstat.heaps_completed << " " << gstat.heaps_discarded << " "
		  << gstat.heaps_open << " "
		  << gstat.bytes_expected << " " << gstat.bytes_received << " "
		  << "trash "
		  << bstat[nbuffers].heaps_completed << " " << bstat[nbuffers].heaps_discarded << " "
		  << bstat[nbuffers].heaps_open << " "
		  << bstat[nbuffers].bytes_expected << " " << bstat[nbuffers].bytes_received << " "
		  << "age "
		  << gstat.heaps_too_old << " " << gstat.heaps_present << " " << gstat.heaps_too_new << " "
		  << "ts "
		  << timestamp_first << " " << timestamp_level << " " << timestamp_last
		  << "\n";
	bstat[replace_slot].reset();
	// getting a new slot
	buffers[replace_slot] = ipcio_open_block_write(dada->data_block, &(indices[replace_slot]));
	// multilog(mlog, LOG_INFO, "got slot %d at %lx", indices[replace_slot], (void*)buffers[replace_slot]);
	std::cout <<  "got slot " <<  indices[replace_slot]  <<  " at " << (void*)buffers[replace_slot] << std::endl;
	payload_base[replace_slot] = buffers[replace_slot];
	sci_base[replace_slot]  = (spead2::s_item_pointer_t*)(buffers[replace_slot] + slot_nbytes - sizeof(spead2::s_item_pointer_t)*nsci*slot_nheaps);
	// clear the side channel items
	if (nsci != 0) memset(sci_base[replace_slot], SCI_EMPTY, sizeof(spead2::s_item_pointer_t)*nsci*slot_nheaps);
	// add the new slot to the internal buffers
	{ // **** MUST BE GUARDED BY SEMAPHORE/MUTEX !!! ****
#ifdef USE_STD_MUTEX
	  std::lock_guard<std::mutex> lock(dest_mutex);
#else
	  dest_sem.get();
#endif
	  buffer_active++;
	  timestamp_level += timestamp_step*slot_ngroups;
	  switch_triggered = (timestamp_level < timestamp_last);
	  lflag = switch_triggered;
#ifndef USE_STD_MUTEX
	  dest_sem.put();
#endif
	}
      } while (lflag);
    } while (true);
  }

  // *******************************************************************
  // ***** class allocator
  // *******************************************************************

  allocator::allocator(std::shared_ptr<options> opts, std::shared_ptr<mkrecv::storage> store) :
    opts(opts),
    store(store)
  {
    std::int64_t i;
    
    heap_nbytes  = opts->heap_nbytes;   // size of a heap in bytes;
    group_nbytes = opts->group_nbytes;  // size of a heap group in bytes
    group_nheaps = opts->group_nheaps;  // number of heaps in a heap group
    timestamp_item = opts->indices[0].item;
    nsci = opts->nsci;
    for (i = 0; i < opts->nsci; i++) {
      scis[i] = opts->scis.at(i);
    }
  }

  allocator::~allocator()
  {
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::cout << "allocator: ";
    et.show();
#endif
  }

  spead2::memory_allocator::pointer allocator::allocate(std::size_t size, void *hint)
  {
    spead2::recv::packet_header    *ph = (spead2::recv::packet_header*)hint;
    spead2::recv::pointer_decoder   decoder(ph->heap_address_bits);
    spead2::s_item_pointer_t        timestamp = -1;
    bool                            use_data;
    std::int64_t                    dest_index = 0;
    std::int64_t                    heap_index = 0;
    char                           *heap_place = NULL;
    spead2::s_item_pointer_t       *sci_place = NULL;
    std::int64_t                    i;

    // **** GUARDED BY SEMAPHORE ****
#ifdef ENABLE_TIMING_MEASUREMENTS
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
    heaps_total++;
    if (has_stopped) {
      use_data = false;
    } else {
      heap_index = opts->p2i(ph, size, timestamp);
      if (heap_index == -1) {
        heap_index = 0;
        use_data = false;
      } else {
        use_data = true;
      }
      if (!use_data) {
        std::cout << "heap ignored heap_cnt " << ph->heap_cnt << " size " << size << " n_items " << ph->n_items << '\n';
      }
    }
    if (use_data) {
      dest_index = store->alloc_data(timestamp, size, heap_place, sci_place);
    } else {
      dest_index = store->alloc_trash(timestamp, size, heap_place, sci_place);
    }
    //std::cout << "heap " << ph->heap_cnt << " size " << size << " nitems " << ph->n_items << " destidx " << dest_index << " heapidx " << heap_index << " timestamp " << timestamp << "\n";
    heap_id[head]        = ph->heap_cnt;
    heap_dest[head]      = dest_index;
    heap_timestamp[head] = timestamp;
    inc(head, MAX_OPEN_HEAPS-1); // (head + 1)%MAX_OPEN_HEAPS;
    heap_place += heap_nbytes*heap_index;
    sci_place  +=        nsci*heap_index;
    for (i = 0; i < nsci; i++) {
      // Put all side-channel items directly into memory
      spead2::item_pointer_t pts = spead2::load_be<spead2::item_pointer_t>(ph->pointers + scis[i]*sizeof(spead2::item_pointer_t));
      sci_place[i] = decoder.get_immediate(pts);
    }
    /*
      if ((dest_index == storage::TRASH_DEST) && (odest_index != storage::TRASH_DEST) && !opts->quiet) {
      std::cout << "HEAP " << ph->heap_cnt << " " << ph->heap_length << " " << ph->payload_offset << " " << ph->payload_length;
      for (i = 0; i < ph->n_items; i++) {
      spead2::item_pointer_t pts = spead2::load_be<spead2::item_pointer_t>(ph->pointers + i*sizeof(spead2::item_pointer_t));
      std::cout << " I[" << i << "] = " << decoder.is_immediate(pts) << " " << decoder.get_id(pts) << " " << decoder.get_immediate(pts);
      }
      std::cout << std::endl;
      }
    */
#ifdef ENABLE_TIMING_MEASUREMENTS
    if (heaps_total == 2000000) et.reset();
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    et.add_et(timing_statistics::ALLOC_TIMING, std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count());
#endif
    return pointer((std::uint8_t*)(heap_place), deleter(shared_from_this(), (void *) std::uintptr_t(size)));
  }

  void allocator::mark(spead2::s_item_pointer_t cnt, bool isok, spead2::s_item_pointer_t reclen)
  {
    std::int64_t             dest_index;
    spead2::s_item_pointer_t timestamp;

    (void)isok;
    {
      // **** GUARDED BY SEMAPHORE ****
#ifdef ENABLE_TIMING_MEASUREMENTS
      std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
      //std::cout << "mark " << cnt << " ok " << isok << " reclen " << reclen << "\n";
      std::int64_t idx = head;
      std::int64_t count = MAX_OPEN_HEAPS;
      dec(idx, MAX_OPEN_HEAPS-1); // heaps + (MAX_OPEN_HEAPS - 1))%MAX_OPEN_HEAPS;
      do {
        if (heap_id[idx] == cnt) {
          dest_index = heap_dest[idx];
          timestamp  = heap_timestamp[idx];
          break;
        }
        dec(idx, MAX_OPEN_HEAPS - 1);
        count--;
      } while (count != 0);
      if (count == 0) {
        std::cout << "ERROR: Cannot find heap with id " << cnt << " in internal map" << '\n';
        dest_index = store->get_trash_dest_index();
        timestamp = 0;
      } else {
        std::int64_t ohead = head;
        dec(ohead,  MAX_OPEN_HEAPS-1); // The index of the latest entry
        if (idx == ohead) {
          // the marked head is the first in the internal map, we can remove this entry
          // by setting the head ine step backwards and overwrite the latest entry next time a heap arrives
          head = ohead;
        }
      }
      //dest_index = heap2dest[cnt];
      //timestamp  = heap2timestamp[cnt];
      //heap2dest.erase(cnt);
      //heap2timestamp.erase(cnt);
#ifdef ENABLE_TIMING_MEASUREMENTS
      std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
      et.add_et(timing_statistics::MARK_TIMING, std::chrono::duration_cast<std::chrono::nanoseconds>( t2 - t1 ).count());
#endif
    }
    store->free_place(timestamp, dest_index, reclen);
  }

  void allocator::request_stop()
  {
    stop = true;
  }

  bool allocator::is_stopped()
  {
    return has_stopped;
  }

  void allocator::free(std::uint8_t *ptr, void *user)
  {
    (void)ptr;
    (void)user;
  }

  // *******************************************************************
  // ***** class stream
  // *******************************************************************

  typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point;

  static time_point start = std::chrono::high_resolution_clock::now();

  void stream::show_heap(const spead2::recv::heap &fheap)
  {
    if (opts->quiet) return;
    time_point now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = now - start;
    std::cout << std::showbase;
    std::cout << "Received heap " << fheap.get_cnt() << " at " << elapsed.count() << '\n';
    if (opts->descriptors) {
      std::vector<spead2::descriptor> descriptors = fheap.get_descriptors();
      for (const auto &descriptor : descriptors) {
        std::cout
          << "Descriptor for " << descriptor.name
          << " (" << std::hex << descriptor.id << ")\n"
          << "  description: " << descriptor.description << '\n'
          << "  format:      [";
        bool first = true;
        for (const auto &field : descriptor.format) {
          if (!first) std::cout << ", ";
          first = false;
          std::cout << '(' << field.first << ", " << field.second << ')';
        }
        std::cout << "]\n";
        std::cout
          << "  dtype:       " << descriptor.numpy_header << '\n'
          << "  shape:       (";
        first = true;
        for (const auto &size : descriptor.shape) {
          if (!first) std::cout << ", ";
          first = false;
          if (size == -1)
            std::cout << "?";
          else
            std::cout << size;
        }
        std::cout << ")\n";
      }
    }
    const auto &items = fheap.get_items();
    for (const auto &item : items) {
      std::cout << std::hex << item.id << std::dec
                << " = [" << item.length << " bytes]\n";
    }
    std::cout << std::noshowbase;
  }

  void stream::heap_ready(spead2::recv::live_heap &&heap)
  {
    ha->mark(heap.get_cnt(), heap.is_contiguous(), heap.get_received_length());
    if (heap.is_contiguous()) {
      //spead2::recv::heap frozen(std::move(heap));
      //show_heap(frozen);
      //rbuffer->mark(heap.get_cnt(), true, heap.get_received_length());
      n_complete++;
    } else {
      //std::cout << "Discarding incomplete heap " << heap.get_cnt() << '\n';
      //rbuffer->mark(heap.get_cnt(), false, heap.get_received_length());
    }
    //if (rbuffer->is_stopped()) stop();
  }

  void stream::stop_received()
  {
    std::cout << "fengine::stop_received()" << '\n';
    spead2::recv::stream::stop_received();
    std::cout << "  after spead2::recv::stream::stop_received()" << '\n';
    stop_promise.set_value();
    std::cout << "  after stop_promise.set_value()" << '\n';
  }

  std::int64_t stream::join()
  {
    std::cout << "stream::join()" << '\n';
    std::future<void> future = stop_promise.get_future();
    future.get();
    return n_complete;
  }

  void stream::set_allocator(std::shared_ptr<mkrecv::allocator> hha)
  {
    ha = hha;
    set_memory_allocator(ha);
  }

  // *******************************************************************
  // ***** class receiver
  // *******************************************************************

  mkrecv::receiver *receiver::instance = NULL;

  static int          g_stopped = 0;
  static std::thread  g_stop_thread;

  void signal_handler(int signalValue)
  {
    std::cout << "received signal " << signalValue << '\n';
    g_stopped++;
    switch (g_stopped) {
    case 1: // first signal: use a thread to stop receiving heaps
      g_stop_thread = std::thread(mkrecv::receiver::request_stop);
      break;
    case 2: // second signal: wait for joining the started thread and exit afterwards
      g_stop_thread.join();
      exit(0);
    default: // third signal: exit immediately
      exit(-1);
    }
  }

  receiver::receiver()
  {
    instance = this;
    opts = NULL;
    storage = NULL;
    thread_pool = NULL;
  }

  receiver::~receiver()
  {
  }

  std::unique_ptr<mkrecv::stream> receiver::create_stream()
  {
    spead2::bug_compat_mask bug_compat = 0;

    std::shared_ptr<mkrecv::allocator> allocator(new mkrecv::allocator(opts, storage));
    allocators.push_back(allocator);
    std::unique_ptr<mkrecv::stream> stream(new mkrecv::stream(opts, thread_pool, bug_compat, opts->nheaps));
    stream->set_allocator(allocator);
    return stream;
  }

  std::unique_ptr<mkrecv::stream> receiver::make_stream(std::vector<std::string>::iterator first_source,
                                                        std::vector<std::string>::iterator last_source)
  {
    using asio::ip::udp;

    std::vector<boost::asio::ip::udp::endpoint>  endpoints;
    std::unique_ptr<mkrecv::stream>           stream;

    stream = create_stream();
    if (opts->memcpy_nt)
      stream->set_memcpy(spead2::MEMCPY_NONTEMPORAL);
    for (std::vector<std::string>::iterator i = first_source; i != last_source; ++i) {
      udp::resolver          resolver(thread_pool->get_io_service());
#ifdef COMBINED_IP_PORT
      std::string::size_type pos = (*i).find_first_of(":");
      std::string            nwadr = std::string((*i).data(), pos);
      std::string            nwport = std::string((*i).data() + pos + 1, (*i).length() - pos - 1);
      udp::resolver::query   query(nwadr, nwport);
      std::cout << "make stream for " << nwadr << ":" << nwport << '\n';
#else
      udp::resolver::query   query((*i), opts->port);
      std::cout << "make stream for " << (*i) << ":" << opts->port << '\n';
#endif
      udp::endpoint          endpoint = *resolver.resolve(query);
      endpoints.push_back(endpoint);
    }
#if SPEAD2_USE_IBV
    if (opts->ibv_if != "") {
      boost::asio::ip::address interface_address = boost::asio::ip::address::from_string(opts->ibv_if);
      stream->emplace_reader<spead2::recv::udp_ibv_reader>(endpoints, interface_address, opts->packet_nbytes, opts->buffer_nbytes, opts->ibv_comp_vector, opts->ibv_max_poll);
    } else {
#endif
      for (std::vector<boost::asio::ip::udp::endpoint>::iterator it = endpoints.begin() ; it != endpoints.end(); ++it) {
        if (opts->udp_if != "") {
          boost::asio::ip::address interface_address = boost::asio::ip::address::from_string(opts->udp_if);
          stream->emplace_reader<spead2::recv::udp_reader>(*it, opts->packet_nbytes, opts->buffer_nbytes, interface_address);
        } else {
          stream->emplace_reader<spead2::recv::udp_reader>(*it, opts->packet_nbytes, opts->buffer_nbytes);
        }
      }
#if SPEAD2_USE_IBV
    }
#endif
    return stream;
  }

  int receiver::execute(int argc, const char **argv)
  {
    opts = std::shared_ptr<mkrecv::options>(new mkrecv::options());
    opts->parse_args(argc, argv);
    std::cout << opts->header << '\n';
    if (opts->quiet) {
      using namespace boost::log;
      core::get()->set_filter(trivial::severity >= trivial::warning);
    }
    thread_pool.reset(new spead2::thread_pool(opts->nthreads));
    signal(SIGINT, signal_handler);
    try {
      storage = std::shared_ptr<mkrecv::storage>(new mkrecv::storage(opts));
      storage->open();
    } catch (std::runtime_error &e) {
      std::cerr << "Cannot connect to DADA Ringbuffer " << opts->dada_key << " exiting..." << '\n';
      exit(0);
    }
    std::int64_t n_complete = 0;
    try {
      if (opts->joint) {
	streams.push_back(make_stream(opts->sources.begin(), opts->sources.end()));
      } else {
	for (auto it = opts->sources.begin(); it != opts->sources.end(); ++it)
	  streams.push_back(make_stream(it, it + 1));
      }
      for (const auto &ptr : streams) {
	auto &stream = dynamic_cast<mkrecv::stream &>(*ptr);
	n_complete += stream.join();
      }
      g_stop_thread.join();
      storage->close();
    } catch (std::runtime_error &e) {
      std::cerr << e.what() << '\n';
    }
    std::cout << "Received " << n_complete << " heaps\n";
    return 0;
  }

  void receiver::request_stop()
  {
    // request a stop from the allocator (fill the current ringbuffer slot)
    instance->storage->request_stop();
    // test if the last ringbuffer slot was filled and the ringbuffer closed
    do {
      if (instance->storage->is_stopped()) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while (1);
    // And now stop all streams
    for (const auto &ptr : instance->streams) {
      auto &stream = dynamic_cast<mkrecv::stream &>(*ptr);
      stream.stop();
    }
  }

}

int main(int argc, const char **argv)
{
  mkrecv::receiver  recv;

  return recv.execute(argc, argv);
}

/*
  ./mkrecv_rnt --header header.cfg --threads 16 \
  --heaps 64 \
  --dada 4 \
  --freq_first 0 --freq_step 256 --freq_count 4 \
  --feng_first 0 --feng_count 4 \
  --time_step 2097152 \
  --port 7148 \
  --udp_if 10.100.207.50 \
  --packet 1500 \
  239.2.1.150 239.2.1.151 239.2.1.152 239.2.1.153


*/
