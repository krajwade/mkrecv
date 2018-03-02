#include <iostream>
#include <utility>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string>
#include <memory>
#include <signal.h>
#include <atomic>
#include <unordered_map>
#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <spead2/common_thread_pool.h>
#include <spead2/recv_udp.h>
#if SPEAD2_USE_NETMAP
# include <spead2/recv_netmap.h>
#endif
#if SPEAD2_USE_IBV
# include <spead2/recv_udp_ibv.h>
#endif
#include <spead2/recv_heap.h>
#include <spead2/recv_live_heap.h>
#include <spead2/recv_ring_stream.h>
#include <spead2/recv_packet.h>
#include <spead2/recv_utils.h>
#include <spead2/common_defines.h>
#include <spead2/common_logging.h>
#include <spead2/common_endian.h>

#include "psrdada_cpp/cli_utils.hpp"
#include "psrdada_cpp/dada_write_client.hpp"
#include "ascii_header.h"

#include "mkrecv_fengine_receiver.h"

int main(int argc, const char **argv)
{
  mkrecv::fengine_receiver  recv;

  return recv.execute(argc, argv);
}

/*
./mkrecv --header header.cfg --threads 16 \
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
