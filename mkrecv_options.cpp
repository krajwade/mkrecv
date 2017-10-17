
#include "mkrecv_options.h"


namespace mkrecv
{

  template<typename T>
  static po::typed_value<T> *make_opt(T &var)
  {
    return po::value<T>(&var)->default_value(var);
  }

  static po::typed_value<bool> *make_opt(bool &var)
  {
    return po::bool_switch(&var)->default_value(var);
  }

  options::options()
  {
    desc.add_options()
      ("quiet", make_opt(quiet), "Only show total of heaps received")
      ("descriptors", make_opt(descriptors), "Show descriptors")
      ("pyspead", make_opt(pyspead), "Be bug-compatible with PySPEAD")
      ("joint", make_opt(joint), "Treat all sources as a single stream")
      ("packet", make_opt(packet), "Maximum packet size to use for UDP")
      ("bind", make_opt(bind), "Bind socket to this hostname")
      ("buffer", make_opt(buffer), "Socket buffer size")
      ("threads", make_opt(threads), "Number of worker threads")
      ("heaps", make_opt(heaps), "Maximum number of in-flight heaps")
      ("memcpy-nt", make_opt(memcpy_nt), "Use non-temporal memcpy")
#if SPEAD2_USE_NETMAP
      ("netmap", make_opt(netmap_if), "Netmap interface")
#endif
#if SPEAD2_USE_IBV
      ("ibv", make_opt(ibv_if), "Interface address for ibverbs")
      ("ibv-vector", make_opt(ibv_comp_vector), "Interrupt vector (-1 for polled)")
      ("ibv-max-poll", make_opt(ibv_max_poll), "Maximum number of times to poll in a row")
#endif
      ("udp_if", make_opt(udp_if), "UDP interface")
      ("freq_first", make_opt(freq_first), "lowest frequency in all incomming heaps")
      ("freq_step", make_opt(freq_step), "difference between consecutive frequencies")
      ("freq_count", make_opt(freq_count), "number of frequency bands")
      ("feng_first", make_opt(feng_first), "lowest fengine id")
      ("feng_count", make_opt(feng_count), "number of fengines")
      ("time_step", make_opt(time_step), "difference between consecutive timestamps")
      ("key", make_opt(key), "PSRDADA ring buffer key")
      ("port", make_opt(port), "Port number")
      ("no-dada", make_opt(no_dada), "put all heaps into trash")
      ;
    hidden.add_options()
      ("source", po::value<std::vector<std::string>>()->composing(), "sources");
    all.add(desc);
    all.add(hidden);

    positional.add("source", -1);
  }

  void options::usage(std::ostream &o)
  {
    o << "Usage: spead2_recv [options] <port>\n";
    o << desc;
  }
  
  void options::parse_args(int argc, const char **argv)
  {
    try
      {
	po::variables_map vm;
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
	if (!vm.count("source"))
	  throw po::error("At least one IP is required");
	sources = vm["source"].as<std::vector<std::string>>();
#if SPEAD2_USE_NETMAP
	if (sources.size() > 1 && netmap_if != "")
	  {
	    throw po::error("--netmap cannot be used with multiple sources");
	  }
#endif
	return;
      }
    catch (po::error &e)
      {
	std::cerr << e.what() << '\n';
	usage(std::cerr);
	std::exit(2);
      }
  }

}
