/*

  gcc -o recv_heaps recv_heaps.c -lpthread

*/


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <endian.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#define MAX_THREADS           4
#define MAX_HEAPS          8192
#define HEAP_HEADER_SIZE               sizeof(uint64_t)
#define HEAP_ITEM_SIZE                 sizeof(uint64_t)
#define MAX_HEAP_PAYLOAD_SIZE  16*1024*sizeof(uint8_t)

typedef struct {
  int                 isok;
  int                 nr;
  int                 sock; // socket connection for receiving heaps from one multicast group
  struct sockaddr_in  adr;
  struct ip_mreq      mreq;
  pthread_t           tid;
  size_t              available;
  size_t              used;
  uint8_t            *data;
  int                 heap_count;
  uint64_t            heap_id[MAX_HEAPS];
  uint64_t            heap_size[MAX_HEAPS];
  uint64_t            heap_received[MAX_HEAPS];
  uint64_t            boards[64];
  uint64_t            channels[4096];
} mcast_context_t;

static int             g_quiet = 0;
static int             g_port = 7148;
static mcast_context_t g_context;

static int create_connection(mcast_context_t *context)
{
  int     rc;
  int     optval    = 1;
  u_char  loop_flag = 1;

  context->isok = 0;
  // create the socket, return 0 in case of error
  context->sock = socket(AF_INET, SOCK_DGRAM,0);
  if (context->sock < 0) {
    fprintf(stderr, "ERROR: can't create socket: %s\n",strerror(errno));
    return 0;
  }
  // bind to the local port
  rc = setsockopt(context->sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  if (rc < 0) {
    fprintf(stderr, "ERROR: can't reuse socket address:%s\n", strerror(errno));
    return 0;
  }
  context->adr.sin_family      = AF_INET;
  context->adr.sin_addr.s_addr = htonl (INADDR_ANY);
  context->adr.sin_port        = htons(g_port);
  rc = bind(context->sock, (struct sockaddr *)&(context->adr), sizeof(context->adr));
  if (rc < 0) {
    fprintf(stderr, "ERROR: can't bind portnumber %d: %s\n", g_port, strerror(errno));
    return 0;
  }
  // activate multicast
  rc = setsockopt(context->sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop_flag, sizeof(loop_flag));
  if (rc < 0) {
    fprintf(stderr, "ERROR: cannot set IP_MULTICAST_LOOP: %s\n", strerror(errno));
    return 0;
  }
  context->available = HEAP_HEADER_SIZE + 15*HEAP_ITEM_SIZE + MAX_HEAP_PAYLOAD_SIZE;
  context->data = (uint8_t *)malloc(context->available);
  if (context->data == NULL) {
    fprintf(stderr, "ERROR: cannot allocate buffer for a SPEAD heap packet: %s\n", strerror(errno));
    return 0;
  }
  context->isok = 1;
  return 1;
}

static int add_connection(mcast_context_t *context, const char *interface_ip, const char *multicast_ip)
{
  int     rc;

  printf("add_connection(.., %s, %s)\n", interface_ip, multicast_ip);
  // join the multicast group:
  context->mreq.imr_multiaddr.s_addr = inet_addr(multicast_ip);
  if(context->mreq.imr_multiaddr.s_addr == -1) {
    fprintf(stderr, "ERROR: cannot decode multicast address %s: %s\n", multicast_ip, strerror(errno));
    return 0;
  }
  context->mreq.imr_interface.s_addr = inet_addr(interface_ip);
  if(context->mreq.imr_interface.s_addr == -1) {
    fprintf(stderr, "ERROR: cannot decode interface address %s: %s\n", interface_ip, strerror(errno));
    return 0;
  }
  rc = setsockopt(context->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &(context->mreq), sizeof(context->mreq));
  if (rc < 0) {
    fprintf(stderr, "ERROR: cannot add multicast membership: %s\n", strerror(errno));
    return 0;
  }
  return 1;
}

static int remove_connection(mcast_context_t *context, const char *interface_ip, const char *multicast_ip)
{
  int     rc;

  // leave the multicast group:
  context->mreq.imr_multiaddr.s_addr = inet_addr(multicast_ip);
  if(context->mreq.imr_multiaddr.s_addr == -1) {
    fprintf(stderr, "ERROR: cannot decode multicast address %s: %s\n", multicast_ip, strerror(errno));
    return 0;
  }
  context->mreq.imr_interface.s_addr = inet_addr(interface_ip);
  if(context->mreq.imr_interface.s_addr == -1) {
    fprintf(stderr, "ERROR: cannot decode interface address %s: %s\n", interface_ip, strerror(errno));
    return 0;
  }
  rc = setsockopt(context->sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &(context->mreq), sizeof(context->mreq));
  if (rc < 0) {
    fprintf(stderr, "ERROR: cannot drop multicast membership: %s\n", strerror(errno));
    return 0;
  }
}

static int delete_connection(mcast_context_t *context)
{
  if (context->data != NULL) {
    free(context->data);
  }
  context->isok = 0;
  return 1;
}

static int process_heap(mcast_context_t *context)
{
  uint64_t   *spead_ptr = (uint64_t*)(context->data);
  uint64_t    spead_header;
  int         nitems, iitem;
  uint64_t    heap_id = 0;
  uint64_t    heap_size = 0;
  uint64_t    heap_received = 0;
  uint64_t    board = 0;
  uint64_t    channel = 0;
  int         index = -1;

  spead_header = (uint64_t)(be64toh(spead_ptr[0]));
  nitems = (int)(spead_header & 0xffff);
  if (!g_quiet) printf("spead %016lx %d items", spead_header, nitems);
  for (iitem = 0; iitem < nitems; iitem++) {
    uint64_t   item;
    int        immediate_flag, item_tag;
    uint64_t   item_value;
    item = (uint64_t)(be64toh(spead_ptr[1 + iitem]));
    if (item & 0x8000000000000000) {
      immediate_flag = 1;
    } else {
      immediate_flag = 0;
    }
    item_tag = (int)((item & 0x7fff000000000000) >> 48);
    item_value = item & 0xffffffffffff;
    if (!g_quiet) printf(" %d %04x %012lx", immediate_flag, item_tag, item_value);
    switch (item_tag) {
    case 1: heap_id = item_value; break;
    case 2: heap_size = item_value; break;
    case 4: heap_received = item_value; break;
    case 0x4101: board = item_value; break;
    case 0x4103: channel = item_value; break;
    default:;
    }
  }
  for (index = context->heap_count - 1; index >= 0; index--) {
    if (context->heap_id[index] == heap_id) break;
    /*
    if (context->heap_id[index] < heap_id - 0x40) {
      index = -1;
      break;
    }
    */
  }
  if ((index == -1) && (context->heap_count < MAX_HEAPS)) {
    index = context->heap_count;
    context->heap_id[index] = heap_id;
    context->heap_size[index] = heap_size;
    context->heap_received[index] = 0;
    context->heap_count++;
  }
  context->heap_received[index] += heap_received;
  context->boards[board] = 1;
  context->channels[channel] = 1;
  if (!g_quiet) printf("\n");
}

static void *receive_heaps(void *arg)
{
  mcast_context_t *context = (mcast_context_t *)arg;
  int              i;

  printf("receive_heaps(%d)\n", context->nr);
  context->heap_count = 0;
  for (i = 0; i < MAX_HEAPS; i++) {
    context->heap_id[i] = 0;
    context->heap_size[i] = 0;
    context->heap_received[i] = 0;
  }
  while (1) {
    uint8_t   *ptr = context->data;
    ssize_t count = recvfrom(context->sock, ptr, context->available, 0, NULL, NULL);
    //if (!g_quiet) printf("packet\n");
    process_heap(context);
    if (context->heap_count == MAX_HEAPS) return NULL;
  }
}

static void usage(char *exename)
{
  printf("usage: $s [-quiet] [-port <port>] <interface_ip> { <multicast_ip> }\n");
  printf("  -quiet         : only a summary after receiving %d heaps is printed\n", MAX_HEAPS);
  printf("  -port <port>   : changes the port from the default %d to a given number\n", g_port);
  printf("  <interface_ip> : ip address (IPv4) of the network interface which will receive the data\n");
  printf("  <multicast_ip> : ip address (IPv4) of a UDP multicast group which send SPEAD heaps\n");
}

int main(int argc, char *argv[])
{
  int    i, j;
  int    isok     = 1;
  int    idx      = 1;
  int    if_index;
  int    mc_index;
  size_t tsize    = 0;
  size_t trec     = 0;

  if (argc <= 2) {
    usage(argv[0]);
    return 0;
  }
  do {
    if (strcmp(argv[idx], "-quiet") == 0) {
      g_quiet = 1;
      idx++;
      continue;
    } 
    if (strcmp(argv[idx], "-port") == 0) {
      idx++;
      g_port = atoi(argv[idx]);
      idx++;
      continue;
    }
    break;
  } while (1);
  for (i = 0; i < 64; i++) {
    g_context.boards[i] = 0;
  }
  for (i = 0; i < 4096; i++) {
    g_context.channels[i] = 0;
  }
  if_index = idx;
  mc_index = idx + 1;
  printf("create all connections\n");
  if (create_connection(&g_context)) {
    for (i = mc_index; i < argc; i++) {
      if (!add_connection(&g_context, argv[if_index], argv[i])) {
	isok = 0;
	break;
      }
    }
  }
  if (isok) {
    // receiving and processing data from all open sockets
    printf("receiving and processing data\n");
    printf("receive and process SPEAD heaps comming from %d multicast group(s)\n", argc - mc_index);
    receive_heaps(&g_context);
  } else {
    // some error occured, do nothing
    printf("some error occured\n");
  }
  printf("closing all connections\n");
  for (i = mc_index; i < argc; i++) {
    if (!remove_connection(&g_context, argv[if_index], argv[i])) {
      isok = 0;
      break;
    }
  }
  delete_connection(&g_context);
  for (j = 1; j < g_context.heap_count - 1; j++) {
    tsize += g_context.heap_size[j];
    trec  += g_context.heap_received[j];
      printf("heap[%05d] %ld size %ld received %ld  = %.1f %%\n",
      j,
      g_context.heap_id[j],
      g_context.heap_size[j],
      g_context.heap_received[j],
      100.0*((double)(g_context.heap_received[j])/(double)(g_context.heap_size[j])));
  }
  printf("nheaps: %d total size %ld total received %ld average received %.1f\n",
	 g_context.heap_count-2,
	 tsize,
	 trec,
	 100.0*(double)trec/(double)tsize);
  for (i = 0; i < 64; i++) {
    if (g_context.boards[i] == 1) {
      printf("board %d\n", i);
    }
  }
  for (i = 0; i < 4096; i++) {
    if (g_context.channels[i] == 1) {
      printf("channel %d\n", i);
    }
  }
}


