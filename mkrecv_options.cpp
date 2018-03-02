

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
    int i;

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
    o << "Usage: mkrecv [options] <port>\n";
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
#if SPEAD2_USE_NETMAP
	if (sources.size() > 1 && netmap_if != "")
	  {
	    throw po::error("--netmap cannot be used with multiple sources");
	  }
#endif
	//if (hdrname != "")
	if (vm.count(HEADER_OPT) != 0)
	  { // read the given template header file and adapt some configuration parameters
	    //hdrname = vm[HEADER_OPT].as<std::string>();
	    std::cout << "try to load file " << hdrname << std::endl;
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
	int length = is.tellg();
	is.seekg (0, is.beg);
	if (length > DADA_DEFAULT_HEADER_SIZE)
	  {
	    std::cerr << "options::load_header(), given file " << hdrname << " contains more than " << DADA_DEFAULT_HEADER_SIZE << " characters, ognoring this file." << std::endl;
	    return;
	  }
	std::cout << "options::load_header(), loading file " << hdrname << std::endl;
	is.read(header, length);
	if (!is)
	  {
	    std::cerr << "error: only " << is.gcount() << " could be read" << std::endl;
	  }
	is.close();
      }
  }

  void options::set_opt(int &val, const char *opt, const char *key)
  {
    std::cout << "set_opt(" << val << ", " << opt << ", " << key << ")" << std::endl;
    std::cout << "  count() = " << vm.count(opt) << std::endl;
    if ((vm.count(opt) == 0) && (key[0] != '\0')) {
      // no option specified, try to get a value from the header
      char sval[1024];
      if (ascii_header_get(header, key, "%s", sval) == -1) {
	std::cout << "  header does not contain a value for " << key << std::endl;
      } else {
	if (strcmp(sval, "unset") == 0) {
	  std::cout << "  header does contain unset for " << key << std::endl;
	} else {
	  if (sscanf(sval, "%d", &val) != 1) {
	    std::cerr << "header contains no integer or unset for key " << key << std::endl;
	  } else {
	    std::cout << "  header contains " << val << std::endl;
	  }
	}
      }
    }
    if (key[0] != '\0') {
      ascii_header_set(header, key, "%d", val);
      if (!check_header()) {
	std::cerr << "ERROR, storing " << key << " with value " << val << " in header failed due to size restrictions. -> incomplete header due to clipping" << std::endl;
      }
    }
  }

  void options::set_opt(std::size_t &val, const char *opt, const char *key)
  {
    std::cout << "set_opt(" << val << ", " << opt << ", " << key << ")" << std::endl;
    std::cout << "  count() = " << vm.count(opt) << std::endl;
    if ((vm.count(opt) == 0) && (key[0] != '\0')) {
      // no option specified, try to get a value from the header
      char sval[1024];
      if (ascii_header_get(header, key, "%s", sval) == -1) {
	std::cout << "  header does not contain a value for " << key << std::endl;
      } else {
	if (strcmp(sval, "unset") == 0) {
	  std::cout << "  header does contain unset for " << key << std::endl;
	} else {
	  if (sscanf(sval, "%ld", &val) != 1) {
	    std::cerr << "header contains no integer or unset for key " << key << std::endl;
	  } else {
	    std::cout << "  header contains " << val << std::endl;
	  }
	}
      }
    }
    if (key[0] != '\0') {
      ascii_header_set(header, key, "%ld", val);
      std::cout << "  header becomes " << val << std::endl;
      if (!check_header()) {
	std::cerr << "ERROR, storing " << key << " with value " << val << " in header failed due to size restrictions. -> incomplete header due to clipping" << std::endl;
      }
    }
  }

  void options::set_opt(std::string &val, const char *opt, const char *key)
  {
    std::cout << "set_opt(" << val << ", " << opt << ", " << key << ")" << std::endl;
    std::cout << "  count() = " << vm.count(opt) << " val = " << val << std::endl;
    if ((vm.count(opt) == 0) && (key[0] != '\0')) {
      // no option specified, try to get a value from the header
      char sval[1024];
      if (ascii_header_get(header, key, "%s", sval) == -1) {
	std::cout << "  header does not contain a value for " << key << std::endl;
      } else {
	if (strcmp(sval, "unset") == 0) {
	  std::cout << "  header does contain unset for " << key << std::endl;
	} else {
	  val = sval;
	  std::cout << "  header contains " << val << std::endl;
	}
      }
    }
    if (key[0] != '\0') {
      if (val.length() == 0) {
	ascii_header_set(header, key, "unset");
	std::cout << "  header becomes unset" << std::endl;
      } else {
	ascii_header_set(header, key, "%s", val.c_str());
	std::cout << "  header becomes " << val << std::endl;
      }
      if (!check_header()) {
	std::cerr << "ERROR, storing " << key << " with value " << val << " in header failed due to size restrictions. -> incomplete header due to clipping" << std::endl;
      }
    }
  }

  void options::set_opt(bool &val, const char *opt, const char *key)
  {
    std::cout << "set_opt(" << val << ", " << opt << ", " << key << ")" << std::endl;
    std::cout << "  count() = " << vm.count(opt) << std::endl;
    if ((vm.count(opt) == 0) && (key[0] != '\0')) {
      // no option specified, try to get a value from the header
      char sval[1024];
      if (ascii_header_get(header, key, "%s", sval) == -1) {
	std::cout << "  header does not contain a value for " << key << std::endl;
      } else {
	if (strcmp(sval, "unset") == 0) {
	  std::cout << "  header does contain unset for " << key << std::endl;
	} else {
	  if (sval[0] == 'F') val = false;
	  if (sval[0] == 'f') val = false;
	  if (sval[0] == 'T') val = true;
	  if (sval[0] == 't') val = true;
	  std::cout << "  header contains " << val << std::endl;
	}
      }
    }
    if (key[0] != '\0') {
      if (val == true) {
	ascii_header_set(header, key, "T");
      } else {
	ascii_header_set(header, key, "F");
      }
      if (!check_header()) {
	std::cerr << "ERROR, storing " << key << " with value " << val << " in header failed due to size restrictions. -> incomplete header due to clipping" << std::endl;
      }
    }
  }

  void options::set_opt(double &val, const char *opt, const char *key)
  {
    std::cout << "set_opt(" << val << ", " << opt << ", " << key << ")" << std::endl;
    std::cout << "  count() = " << vm.count(opt) << std::endl;
    if ((vm.count(opt) == 0) && (key[0] != '\0')) {
      // no option specified, try to get a value from the header
      char sval[1024];
      if (ascii_header_get(header, key, "%s", sval) == -1) {
	std::cout << "  header does not contain a value for " << key << std::endl;
      } else {
	if (strcmp(sval, "unset") == 0) {
	  std::cout << "  header does contain unset for " << key << std::endl;
	} else {
	  if (sscanf(sval, "%lf", &val) != 1) {
	    std::cerr << "header contains no float or unset for key " << key << std::endl;
	  } else {
	    std::cout << "  header contains " << val << std::endl;
	  }
	}
      }
    }
    if (key[0] != '\0') {
      ascii_header_set(header, key, "%lf", val);
      if (!check_header()) {
	std::cerr << "ERROR, storing " << key << " with value " << val << " in header failed due to size restrictions. -> incomplete header due to clipping" << std::endl;
      }
    }
  }

  void options::use_sources(std::vector<std::string> &val, const char *opt, const char *key)
  {
    if ((vm.count(opt) == 0) && (key[0] != '\0'))
      {
	char cs[1024];
	ascii_header_get(header, key, "%s", cs);
	cs[sizeof(cs)-1] = '\0';
	std::string str(cs);
	std::string::size_type pos, lastPos = 0, length = str.length();
	while(lastPos < length + 1) {
	  pos = str.find_first_of(",", lastPos);
	  if(pos == std::string::npos) {
	    pos = length;
	  }
	  if(pos != lastPos)
	    val.push_back(std::string(str.data()+lastPos, pos-lastPos ));
	  lastPos = pos + 1;
	}
      }
  }

  void options::update_sources()
  {
    for (auto it = sources.begin(); it != sources.end(); ++it)
      {
	std::string::size_type pos = (*it).find_first_of(":");
	std::string            used_source;
	if (pos == std::string::npos)
	  { // no port number given in source string, use opts.port
	    used_source = *it;
	    used_source.append(":");
	    used_source.append(port);
	  }
	else
	  { // source string contains <ip>:<port> -> split in both parts
	    std::string nwadr = std::string((*it).data(), pos);
	    std::string nwport = std::string((*it).data() + pos + 1, (*it).length() - pos - 1);
	    if (port != "")
	      {
		nwport = port;
	      }
	    used_source = nwadr;
	    used_source.append(":");
	    used_source.append(nwport);
	  }
	(*it).assign(used_source);
	if (used_sources.length() != 0)
	  {
	    used_sources.append(",");
	  }
	used_sources.append(used_source);
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
    desc.add_options()
      ("help",           "show this text")
      // optional header file contain configuration options and additional information
      (HEADER_OPT,       make_opt(hdrname),         HEADER_DESC)
      // some flags
      (QUIET_OPT,        make_opt(quiet),           QUIET_DESC)
      (DESCRIPTORS_OPT,  make_opt(descriptors),     DESCRIPTORS_DESC)
      (PYSPEAD_OPT,      make_opt(pyspead),         PYSPEAD_DESC)
      (JOINT_OPT,        make_opt(joint),           JOINT_DESC)
      // some options, default values should be ok to use, will _not_ go into header
      (PACKET_OPT,       make_opt(packet),          PACKET_DESC)
      (BUFFER_OPT,       make_opt(buffer),          BUFFER_DESC)
      (NTHREADS_OPT,     make_opt(threads),         NTHREADS_DESC)
      (NHEAPS_OPT,       make_opt(heaps),           NHEAPS_DESC)
      ("memcpy-nt",      make_opt(memcpy_nt),       "Use non-temporal memcpy")
      // DADA ringbuffer related stuff
      (DADA_KEY_OPT,     make_opt(key),             DADA_KEY_DESC)
      (DADA_MODE_OPT,    make_opt(dada_mode),       DADA_MODE_DESC)
      // network configuration
#if SPEAD2_USE_NETMAP
      (NETMAP_IF_OPT,    make_opt(netmap_if),       NETMAP_IF_DESC)
#endif
#if SPEAD2_USE_IBV
      (IBV_IF_OPT,       make_opt(ibv_if),          IBV_IF_DESC)
      (IBV_VECTOR_OPT,   make_opt(ibv_comp_vector), IBV_VECTOR_DESC)
      (IBV_MAX_POLL_OPT, make_opt(ibv_max_poll),    IBV_MAX_POLL_DESC)
#endif
      (UDP_IF_OPT,       make_opt(udp_if),          UDP_IF_DESC)
      (PORT_OPT,         make_opt(port),            PORT_DESC)
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
    std::size_t    ival;

    /* Flags, therefore all default values are false */
    set_opt(quiet,           QUIET_OPT, QUIET_KEY);
    set_opt(descriptors,     DESCRIPTORS_OPT, DESCRIPTORS_KEY);
    set_opt(pyspead,         PYSPEAD_OPT, PYSPEAD_KEY);
    set_opt(joint,           JOINT_OPT, JOINT_KEY);
    /* the following options should have sufficient default values */
    set_opt(packet,          PACKET_OPT, PACKET_KEY);
    set_opt(buffer,          BUFFER_OPT, BUFFER_KEY);
    set_opt(threads,         NTHREADS_OPT, NTHREADS_KEY);
    set_opt(heaps,           NHEAPS_OPT, NHEAPS_KEY);
    /* The following options describe the DADA ringbuffer use */
    set_opt(key,             DADA_KEY_OPT, DADA_KEY_KEY);
    set_opt(dada_mode,       DADA_MODE_OPT, DADA_MODE_KEY);
    /* The following options describe the connection to the F-Engines (network) */
#if SPEAD2_USE_NETMAP
    set_opt(netmap_if,       NETMAP_IF_OPT, NETMAP_IF_KEY);
#endif
#if SPEAD2_USE_IBV
    set_opt(ibv_if,          IBV_IF_OPT, IBV_IF_KEY);
    set_opt(ibv_comp_vector, IBV_VECTOR_OPT, IBV_VECTOR_KEY);
    set_opt(ibv_max_poll,    IBV_MAX_POLL_OPT, IBV_MAX_POLL_KEY);
#endif
    set_opt(udp_if,          UDP_IF_OPT, UDP_IF_KEY);
    set_opt(port,            PORT_OPT, PORT_KEY);
    use_sources(sources,     SOURCES_OPT, SOURCES_KEY);
    update_sources();
    if (used_sources.length() == 0) {
      ascii_header_set(header, SOURCES_KEY, "unset");
      std::cout << "  header becomes unset" << std::endl;
    } else {
      ascii_header_set(header, SOURCES_KEY, "%s", used_sources.c_str());
      std::cout << "  header becomes " << used_sources << std::endl;
    }
    if (!check_header()) {
      std::cerr << "ERROR, storing " << SOURCES_KEY << " with value " << used_sources << " in header failed due to size restrictions. -> incomplete header due to clipping" << std::endl;
    }
  }

}
