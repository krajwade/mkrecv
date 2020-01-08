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

#include "mkrecv_options.h"
#include "mkrecv_allocator.h"

#define NLOOPS 1000000
#define NFUNC  8

int main(int argc, const char **argv)
{
  mkrecv::options    opts;
  mkrecv::index_part indices[MAX_INDEXPARTS];
  std::size_t        nindices = 0;
  std::size_t        heap_size;   // size of a heap in bytes
  std::size_t        heap_count = 1;  // number of heaps inside one group
  std::size_t        index_size = 1;
  int                i;

  // parse the command line options
  opts.parse_args(argc, argv);
  std::cout << opts.header << '\n';
  
  // copy index_option into a local index_part
  nindices = opts.nindices;
  for (i = nindices - 1; i >= 0; i--) {
    indices[i].set(opts.indices[i], index_size);
    index_size *= indices[i].count;
  }
  heap_count = index_size;
  heap_size = opts.heap_size;
  std::cout << "heap: size " << heap_size << " count " << heap_count << std::endl;

  std::cout << "IDX 2: " <<   0 << " -> " << indices[1].v2i(  0) << '\n';
  std::cout << "IDX 2: " <<   1 << " -> " << indices[1].v2i(  1) << '\n';
  std::cout << "IDX 2: " <<   2 << " -> " << indices[1].v2i(  2) << '\n';
  std::cout << "IDX 2: " <<   3 << " -> " << indices[1].v2i(  3) << '\n';
  std::cout << "IDX 2: " <<  31 << " -> " << indices[1].v2i( 31) << '\n';
  std::cout << "IDX 3: " << 164 << " -> " << indices[2].v2i(164) << '\n';
  std::cout << "IDX 3: " << 168 << " -> " << indices[2].v2i(168) << '\n';
  std::cout << "IDX 3: " << 172 << " -> " << indices[2].v2i(172) << '\n';
  std::cout << "IDX 3: " << 228 << " -> " << indices[2].v2i(228) << '\n';
  std::cout << "IDX 3: " <<  31 << " -> " << indices[2].v2i( 31) << '\n';
  std::cout << "IDX 3: " << 173 << " -> " << indices[2].v2i(173) << '\n';
  std::cout << "IDX 3: " << 320 << " -> " << indices[2].v2i(320) << '\n';
  std::cout << "IDX 3: " <<   0 << " -> " << indices[2].v2i( 0) << '\n';
  std::cout << "IDX 3: " <<   7 << " -> " << indices[2].v2i( 7) << '\n';
  std::cout << "IDX 3: " <<   8 << " -> " << indices[2].v2i( 8) << '\n';
  std::cout << "IDX 3: " <<  16 << " -> " << indices[2].v2i(16) << '\n';

  std::chrono::high_resolution_clock::time_point hmt1 = std::chrono::high_resolution_clock::now();
  for (i = 0; i < NLOOPS; i++) {
    indices[1].v2i(  0, true);
    indices[1].v2i(  1, true);
    indices[1].v2i(  2, true);
    indices[1].v2i(  3, true);
    //indices[1].v2i( 31, true);
    indices[2].v2i(164, true);
    indices[2].v2i(168, true);
    indices[2].v2i(172, true);
    indices[2].v2i(228, true);
    //indices[2].v2i( 31, true);
    //indices[2].v2i(173, true);
    //indices[2].v2i(320, true);
  }
  std::chrono::high_resolution_clock::time_point hmt2 = std::chrono::high_resolution_clock::now();

  std::chrono::high_resolution_clock::time_point dmt1 = std::chrono::high_resolution_clock::now();
  for (i = 0; i < NLOOPS; i++) {
    indices[1].v2i(  0);
    indices[1].v2i(  1);
    indices[1].v2i(  2);
    indices[1].v2i(  3);
    //indices[1].v2i( 31);
    indices[2].v2i(164);
    indices[2].v2i(168);
    indices[2].v2i(172);
    indices[2].v2i(228);
    //indices[2].v2i( 31);
    //indices[2].v2i(173);
    //indices[2].v2i(320);
  }
  std::chrono::high_resolution_clock::time_point dmt2 = std::chrono::high_resolution_clock::now();

  std::cout << "total time: loops " << NLOOPS
	    << " hash map "
	    << std::chrono::duration_cast<std::chrono::nanoseconds>( hmt2 - hmt1 ).count()
	    << " fc "
	    << (double)(std::chrono::duration_cast<std::chrono::nanoseconds>( hmt2 - hmt1 ).count())/((double)NFUNC*NLOOPS)
	    << " direct map "
	    << std::chrono::duration_cast<std::chrono::nanoseconds>( dmt2 - dmt1 ).count()
	    << " fc "
	    << (double)(std::chrono::duration_cast<std::chrono::nanoseconds>( dmt2 - dmt1 ).count())/((double)NFUNC*NLOOPS)
	    << " ns\n";

  return 0;
}

/*
./mkrecv_test --nindices 3 --idx1-item 0 --idx1-step 4096 --idx2-item 1 --idx2-list 0,1,2,3 --idx3-item 3 --idx3-list 41,43,45,57

./mkrecv_test --nindices 3 --idx1-item 0 --idx1-step 4096 --idx2-item 1 --idx2-list 0,1,2,3 --idx3-item 3 --idx3-list 164,168,172,228

./mkrecv_test --nindices 3 --idx1-item 0 --idx1-step 4096 --idx2-item 1 --idx2-list 0,1,2,3 --idx3-item 3 --idx3-list 0,8,16,24,32,40,48,56

 */
