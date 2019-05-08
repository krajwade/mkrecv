

#include <string.h>

#include <iostream>     // std::cout
#include <fstream>      // std::ifstream

#include "ascii_header.h"

#include "mkrecv_options.h"


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
    if (!ready)
      {
	ready = true;
	create_args();
      }
    all.add(desc);
    all.add(hidden);
    try
      {
	po::store(po::command_line_parser(argc, argv)
		  .style(po::command_line_style::default_style & ~po::command_line_style::allow_guessing)
		  .options(all)
		  .positional(positional)
		  .run(), vm);
	po::notify(vm);
	if (vm.count("help"))
	  {
	    usage(std::cout);
	    std::exit(0);
	  }
	/*
	if (!vm.count("source"))
	  throw po::error("At least one IP is required");
	*/
	if (vm.count("source"))
	  {
	    sources = vm["source"].as<std::vector<std::string>>();
	  }
	//if (hdrname != "")
	if (vm.count(HEADER_OPT) != 0)
	  { // read the given template header file and adapt some configuration parameters
	    //hdrname = vm[HEADER_OPT].as<std::string>();
	    //std::cout << "try to load file " << hdrname << '\n';
	    load_header();
	  }
	apply_header(); // use values from header or default values
	return;
      }
    catch (po::error &e)
      {
	std::cerr << e.what() << '\n';
	usage(std::cerr);
	std::exit(2);
      }
  }

  void options::load_header()
  {
    std::ifstream   is(hdrname, std::ifstream::binary);

    if (is)
      {
	// get length of file:
	is.seekg (0, is.end);
	uint64_t length = is.tellg();
	is.seekg (0, is.beg);
	if (length > DADA_DEFAULT_HEADER_SIZE)
	  {
	    std::cerr << "options::load_header(), given file " << hdrname << " contains more than " << DADA_DEFAULT_HEADER_SIZE << " characters, ognoring this file." << '\n';
	    return;
	  }
	//std::cout << "options::load_header(), loading file " << hdrname << '\n';
	is.read(header, length);
	if (!is)
	  {
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
    if (!check_header())
      {
	std::cerr << "ERROR, storing " << SAMPLE_CLOCK_START_KEY << " with value " << timestamp << " in header failed due to size restrictions. -> incomplete header due to clipping" << '\n';
      }
    ascii_header_set(header, UTC_START_KEY, "%s", utc_string);
    if (!check_header())
      {
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
  void options::finalize_parameter(std::string &val, const char *opt, const char *key)
  {
    if (!quiet) std::cout << "finalize_parameter(" << val << ", " << opt << ", " << key << ")";
    if (vm.count(opt) != 0) { // Check if the parameter is given as program option
      // -> use the program option value already given in val and store it in the header file
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
	  if (!quiet) std::cout << " header unset, default for " << opt << " is " << val;
	} else {
	  // -> use the value from the header file
	  val = sval;
	  if (!quiet) std::cout << " header for " << opt << " is " << val;
	}
      } else {
	if (!quiet) std::cout << " default for " << opt << " is " << val;
      }
    }
    if (!quiet) std::cout << " -> " << val << '\n';
  }

  bool options::parse_fixnum(int &val, std::string &val_str)
  {
    try {
      if (val_str.compare(0,2,"0x") == 0)
	{
	  val = std::stoi(std::string(val_str.begin() + 2, val_str.end()), nullptr, 16);
	}
      else if (val_str.compare(0,2,"0b") == 0)
	{
	  val = std::stoi(std::string(val_str.begin() + 2, val_str.end()), nullptr, 2);
	}
      else
	{
	  val = std::stoi(val_str, nullptr, 10);
	}
      return true;
    } catch (std::exception& e) {
      return false;
    }
  }
  
  bool options::parse_fixnum(spead2::s_item_pointer_t &val, std::string &val_str)
  {
    try {
      if (val_str.compare(0,2,"0x") == 0)
	{
	  val = std::stol(std::string(val_str.begin() + 2, val_str.end()), nullptr, 16);
	}
      else if (val_str.compare(0,2,"0b") == 0)
	{
	  val = std::stol(std::string(val_str.begin() + 2, val_str.end()), nullptr, 2);
	}
      else
	{
	  val = std::stol(val_str, nullptr, 10);
	}
      return true;
    } catch (std::exception& e) {
      return false;
    }
  }
  
  bool options::parse_fixnum(std::size_t &val, std::string &val_str)
  {
    try {
      if (val_str.compare(0,2,"0x") == 0)
	{
	  val = std::stol(std::string(val_str.begin() + 2, val_str.end()), nullptr, 16);
	}
      else if (val_str.compare(0,2,"0b") == 0)
	{
	  val = std::stol(std::string(val_str.begin() + 2, val_str.end()), nullptr, 2);
	}
      else
	{
	  val = std::stol(val_str, nullptr, 10);
	}
      return true;
    } catch (std::exception& e) {
      return false;
    }
  }

  void options::parse_parameter(std::string &val, const char *opt, const char *key)
  {
    finalize_parameter(val, opt, key);
    //std::cout << opt << "/" << key << " = " << val << '\n';
  }

  void options::parse_parameter(int &val, std::string &val_str, const char *opt, const char *key)
  {
    finalize_parameter(val_str, opt, key);
    // we have the final value in val_str -> convert it into an integer number according to an optional prefix
    if (!parse_fixnum(val, val_str))
      {
	std::cerr << "Exception: cannot convert " << val_str << " into int for option " << opt << "/" << key << '\n';
	// put the default value already given in val into the header (the current value cannot be converted into std::size_t)
	if (key[0] != '\0') {
	  ascii_header_set(header, key, "%d", val);
	  //std::cout << "  header becomes " << val << '\n';
	  if (!check_header()) {
	    std::cerr << "ERROR, storing " << key << " with value " << val << " in header failed due to size restrictions. -> incomplete header due to clipping" << '\n';
	  }
	}
      }
    if (!quiet) std::cout << opt << "/" << key << " = " << val << '\n';
  }

  void options::parse_parameter(std::size_t &val, std::string &val_str, const char *opt, const char *key)
  {
    finalize_parameter(val_str, opt, key);
    // we have the final value in val_str -> convert it into an integer number according to an optional prefix
    if (!parse_fixnum(val, val_str))
      {
	std::cerr << "Exception: cannot convert " << val_str << " into int for option " << opt << "/" << key << '\n';
	// put the default value already given in val into the header (the current value cannot be converted into std::size_t)
	if (key[0] != '\0') {
	  ascii_header_set(header, key, "%ld", val);
	  //std::cout << "  header becomes " << val << '\n';
	  if (!check_header()) {
	    std::cerr << "ERROR, storing " << key << " with value " << val << " in header failed due to size restrictions. -> incomplete header due to clipping" << '\n';
	  }
	}
      }
    if (!quiet) std::cout << opt << "/" << key << " = " << val << '\n';
  }

  void options::parse_parameter(double &val, std::string &val_str, const char *opt, const char *key)
  {
    finalize_parameter(val_str, opt, key);
    // we have the final value in val_str -> convert it into an integer number according to an optional prefix
    try {
      val = std::stod(val_str, nullptr);
    } catch (std::exception& e) {
      std::cerr << "Exception: " << e.what() << " cannot convert " << val_str << " into double for option " << opt << "/" << key << '\n';
      // put the default value already given in val into the header (the current value cannot be converted into std::size_t)
      if (key[0] != '\0') {
	ascii_header_set(header, key, "%f", val);
	//std::cout << "  header becomes " << val << '\n';
	if (!check_header()) {
	  std::cerr << "ERROR, storing " << key << " with value " << val << " in header failed due to size restrictions. -> incomplete header due to clipping" << '\n';
	}
      }
    }
    if (!quiet) std::cout << opt << "/" << key << " = " << val << '\n';
  }

  /*
    Parse a list of integer values where each value is either a direct value or a range definition.
    All integer values (direct or in a range definition) are either a decimal, hexadecimal or binary number.
    The syntax for an integer list is:
    <list> := "unset" | ( <element> { "," <element> } ) .
    <element> := <number> | ( <first> ":" <last> ":" <step> ) .
    <number>  := <decimal> | <hexadecimal> | <binary> .
   */
  void options::parse_parameter(std::vector<spead2::s_item_pointer_t> &val, std::string &val_str, const char *opt, const char *key)
  {
    std::string::size_type str_from = 0, str_to, str_length;

    finalize_parameter(val_str, opt, key);
    // we have the final value in val_str -> convert it into an list of integer number according to an optional prefix and with range support
    val.clear();
    str_length = val_str.length();
    while(str_from < str_length + 1) {
      str_to = val_str.find_first_of(",", str_from);
      if(str_to == std::string::npos) str_to = str_length;
      if(str_to == str_from) break;
      std::string               el_str(val_str.data() + str_from, str_to - str_from);
      std::string::size_type    el_from = 0, el_to, el_length;
      spead2::s_item_pointer_t  vals[3];
      int                       nparts = 0;
      el_length = el_str.length();
      while((el_from < el_length + 1) && (nparts != 3)) {
	el_to = el_str.find_first_of(":", el_from);
	if(el_to == std::string::npos) el_to = el_length;
	if(el_to == el_from) break;
	std::string hel(el_str.data() + el_from, el_to - el_from);
	if (!parse_fixnum(vals[nparts], hel)) {
	  std::cerr << "Exception: cannot convert " << hel << " at position " << str_from << " into spead2::s_item_pointer_t for option " << opt << "/" << key << '\n';
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
      for (i = 0; i < val.size(); i++)
	{
	  std::cout << " " << val.at(i) << "->" << i;
	}
      std::cout << '\n';
    }
  }

  void options::parse_parameter(std::vector<std::size_t> &val, std::string &val_str, const char *opt, const char *key)
  {
    std::string::size_type str_from = 0, str_to, str_length;

    finalize_parameter(val_str, opt, key);
    // we have the final value in val_str -> convert it into an list of integer number according to an optional prefix and with range support
    val.clear();
    str_length = val_str.length();
    while(str_from < str_length + 1) {
      str_to = val_str.find_first_of(",", str_from);
      if(str_to == std::string::npos) str_to = str_length;
      if(str_to == str_from) break;
      std::string               el_str(val_str.data() + str_from, str_to - str_from);
      std::string::size_type    el_from = 0, el_to, el_length;
      std::size_t               vals[3];
      int                       nparts = 0;
      el_length = el_str.length();
      while((el_from < el_length + 1) && (nparts != 3)) {
	el_to = el_str.find_first_of(":", el_from);
	if(el_to == std::string::npos) el_to = el_length;
	if(el_to == el_from) break;
	std::string hel(el_str.data() + el_from, el_to - el_from);
	if (!parse_fixnum(vals[nparts], hel)) {
	  std::cerr << "Exception: cannot convert " << hel << " at position " << str_from << " into std::size_t for option " << opt << "/" << key << '\n';
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
      for (i = 0; i < val.size(); i++)
	{
	  std::cout << " " << val.at(i) << "->" << i;
	}
      std::cout << '\n';
    }
  }

  /*
    two kinds of notation:
    1. <a0> "." <a1> "." <a2> "." <a3> [ "+" <offset> [ "|" <step> ] ] [ ":" <port> ] .
    2. <a0> "." <a1> "." <a2> "." <a3> [ "+" <offset> [ ":" <step> ] ] .
   */
  void options::parse_parameter(std::vector<std::string> &val, std::string &val_str, const char *opt, const char *key)
  {
    std::string::size_type str_from = 0, str_to, str_length;

    finalize_parameter(val_str, opt, key);
    // we have the final value in val_str -> convert it into an list of IPs allowing "i.j.k.l+n" notation
    val.clear();
    str_length = val_str.length();
    while(str_from < str_length + 1) {
      str_to = val_str.find_first_of(",", str_from);
      if(str_to == std::string::npos) str_to = str_length;
      if(str_to == str_from) break;
      std::string               el_str(val_str.data() + str_from, str_to - str_from);
      std::string::size_type    el_from = 0, el_to, el_length;
      int                       vals[8];
      int                       nadr = 0;
      int                       ipart;
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
	snprintf(ip_adr, sizeof(ip_adr), "%d.%d.%d.%d", vals[0], vals[1], vals[2], vals[3]);
	ip_str = ip_adr;
#ifdef COMBINED_IP_PORT
	ip_str += ":";
	if (has_port) {
	  char ip_port[16];
	  snprintf(ip_port, sizeof(ip_adr), "%d", vals[6]);
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
    int i;

    desc.add_options()
      ("help",            "show this text")
      // optional header file contain configuration options and additional information
      (HEADER_OPT,        make_opt(hdrname),             HEADER_DESC)
      // some flags
      (QUIET_OPT,         make_opt(quiet),               QUIET_DESC)
      (DESCRIPTORS_OPT,   make_opt(descriptors),         DESCRIPTORS_DESC)
      (PYSPEAD_OPT,       make_opt(pyspead),             PYSPEAD_DESC)
      (JOINT_OPT,         make_opt(joint),               JOINT_DESC)
      // some options, default values should be ok to use, will _not_ go into header
      (PACKET_OPT,        make_opt(packet_str),          PACKET_DESC)
      (BUFFER_OPT,        make_opt(buffer_str),          BUFFER_DESC)
      (NTHREADS_OPT,      make_opt(threads_str),         NTHREADS_DESC)
      (NHEAPS_OPT,        make_opt(heaps_str),           NHEAPS_DESC)
      ("memcpy-nt",       make_opt(memcpy_nt),       "Use non-temporal memcpy")
      // DADA ringbuffer related stuff
      (DADA_MODE_OPT,     make_opt(dada_mode_str),       DADA_MODE_DESC)
      (DADA_KEY_OPT,      make_opt(dada_key),            DADA_KEY_DESC)
      // network configuration
#if SPEAD2_USE_IBV
      (IBV_IF_OPT,        make_opt(ibv_if),              IBV_IF_DESC)
      (IBV_VECTOR_OPT,    make_opt(ibv_comp_vector_str), IBV_VECTOR_DESC)
      (IBV_MAX_POLL_OPT,  make_opt(ibv_max_poll_str),    IBV_MAX_POLL_DESC)
#endif
      (UDP_IF_OPT,        make_opt(udp_if),              UDP_IF_DESC)
      (PORT_OPT,          make_opt(port),                PORT_DESC)
      (SYNC_EPOCH_OPT,    make_opt(sync_epoch_str),      SYNC_EPOCH_DESC)
      (SAMPLE_CLOCK_OPT,  make_opt(sample_clock_str),    SAMPLE_CLOCK_DESC)
      (HEAP_SIZE_OPT,     make_opt(heap_size_str),       HEAP_SIZE_DESC)
      (NGROUPS_DATA_OPT,  make_opt(ngroups_data_str),    NGROUPS_DATA_DESC)
      (NGROUPS_TEMP_OPT,  make_opt(ngroups_temp_str),    NGROUPS_TEMP_DESC)
      (LEVEL_DATA_OPT,    make_opt(level_data_str),      LEVEL_DATA_DESC)
      (LEVEL_TEMP_OPT,    make_opt(level_temp_str),      LEVEL_TEMP_DESC)
      (NHEAPS_SWITCH_OPT, make_opt(nheaps_switch_str),   NHEAPS_SWITCH_DESC)
      ;
    // index calculation
    desc.add_options()
      (NINDICES_OPT,     make_opt(nindices_str),         NINDICES_DESC)
      ;
    for (i = 0; i < MAX_INDEXPARTS; i++)
      {
	char olabel[32];
	char odesc[255];
	snprintf(olabel, sizeof(olabel) - 1, IDX_ITEM_OPT,  i+1);
	snprintf(odesc,  sizeof(odesc)  - 1, IDX_ITEM_DESC, i+1);
	desc.add_options()(olabel, make_opt(indices[i].item_str), odesc);
	if (i == 0)
	  {
	    desc.add_options()(IDX_STEP_OPT, make_opt(indices[i].step_str), IDX_STEP_DESC);
	    desc.add_options()(IDX_MOD_OPT,  make_opt(indices[i].mod_str),  IDX_MOD_DESC);
	  }
	else
	  {
	    snprintf(olabel, sizeof(olabel) - 1, IDX_MASK_OPT,  i+1);
	    snprintf(odesc,  sizeof(odesc)  - 1, IDX_MASK_DESC, i+1);
	    desc.add_options()(olabel, make_opt(indices[i].mask_str), odesc);
	    snprintf(olabel, sizeof(olabel) - 1, IDX_LIST_OPT,  i+1);
	    snprintf(odesc,  sizeof(odesc)  - 1, IDX_LIST_DESC, i+1);
	    desc.add_options()(olabel, make_opt(indices[i].list), odesc);
	  }
      }
    desc.add_options()
      (SCI_LIST_OPT,  make_opt(sci_list), SCI_LIST_DESC)
      ;
    hidden.add_options()
      // network configuration
      (SOURCES_OPT,      po::value<std::vector<std::string>>()->composing(), SOURCES_DESC);
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
    int            i;

    /* the following options should have sufficient default values */
    parse_parameter(packet,          packet_str,          PACKET_OPT, PACKET_KEY);
    parse_parameter(buffer,          buffer_str,          BUFFER_OPT, BUFFER_KEY);
    parse_parameter(threads,         threads_str,         NTHREADS_OPT, NTHREADS_KEY);
    parse_parameter(heaps,           heaps_str,           NHEAPS_OPT, NHEAPS_KEY);
    /* The following options describe the DADA ringbuffer use */
    parse_parameter(dada_mode,       dada_mode_str,       DADA_MODE_OPT, DADA_MODE_KEY);
    parse_parameter(dada_key,                             DADA_KEY_OPT, DADA_KEY_KEY);
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
    parse_parameter(heap_size,       heap_size_str,       HEAP_SIZE_OPT, HEAP_SIZE_KEY);
    parse_parameter(ngroups_data,    ngroups_data_str,    NGROUPS_DATA_OPT, NGROUPS_DATA_KEY);
    parse_parameter(ngroups_temp,    ngroups_temp_str,    NGROUPS_TEMP_OPT, NGROUPS_TEMP_KEY);
    parse_parameter(level_data,      level_data_str,      LEVEL_DATA_OPT, LEVEL_DATA_KEY);
    parse_parameter(level_temp,      level_temp_str,      LEVEL_TEMP_OPT, LEVEL_TEMP_KEY);
    parse_parameter(nheaps_switch,   nheaps_switch_str,   NHEAPS_SWITCH_OPT, NHEAPS_SWITCH_KEY);
    parse_parameter(nindices,        nindices_str,        NINDICES_OPT, NINDICES_KEY);
    if (nindices >= MAX_INDEXPARTS) nindices = MAX_INDEXPARTS-1;
    for (i = 0; i < nindices; i++)
      {
	char iopt[32];
	char ikey[32];
	snprintf(iopt, sizeof(iopt) - 1, IDX_ITEM_OPT, i+1);
	snprintf(ikey, sizeof(ikey) - 1, IDX_ITEM_KEY, i+1);
	parse_parameter(indices[i].item, indices[i].item_str, iopt, ikey);
	if (i == 0)
	  {
	    parse_parameter(indices[i].step, indices[i].step_str, IDX_STEP_OPT, IDX_STEP_KEY);
	    parse_parameter(indices[i].mod,  indices[i].mod_str,  IDX_MOD_OPT,  IDX_MOD_KEY);
	  }
	else
	  {
	    snprintf(iopt, sizeof(iopt) - 1, IDX_MASK_OPT, i+1);
	    snprintf(ikey, sizeof(ikey) - 1, IDX_MASK_KEY, i+1);
	    parse_parameter(indices[i].mask, indices[i].mask_str, iopt, ikey);
	    snprintf(iopt, sizeof(iopt) - 1, IDX_LIST_OPT, i+1);
	    snprintf(ikey, sizeof(ikey) - 1, IDX_LIST_KEY, i+1);
	    parse_parameter(indices[i].values, indices[i].list, iopt, ikey);
	  }
      }
    parse_parameter(scis, sci_list, SCI_LIST_OPT, SCI_LIST_KEY);
    nsci = scis.size();
    parse_parameter(sources, sources_str, SOURCES_OPT, SOURCES_KEY);
  }

}
