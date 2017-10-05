/*

vi decode_heap.c

gcc -o decode_heap decode_heap.c

chmod +x decode_heap

./decode_heap /dev/shm/capture_239.2.1.151.dump | more

*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/*
0000000 d4 c3 b2 a1 02 00 04 00 00 00 00 00 00 00 00 00
       |>         <| Magic
                   |>         <| Version (Major, minor)
                               |>         <| Time zone offset
                                           |>         <| Time stamp accuracy
0000020 ff ff 00 00 01 00 00 00 de 12 c9 59 44 84 03 00
       |>         <| Snapshot length
                   |>         <| Link-layer header type
                               |>         <| timestamp seconds
                                           |>         <| timestamp nanoseconds
0000040 8a 04 00 00 8a 04 00 00 01 00 5e 02 01 96 02 02
       |>         <| length of captured data
                   |>         <| untruncated length of packet data
0000060 c0 a8 01 9a 08 00 45 00 04 7c 00 00 40 00 ff 11
                         |><| Version + Internet header length
                            |><| Type of service
                               |>   <| Total length (Header + body)
                                     |>   <| Identification
                                           |>   <| Flags + fragment offset
                                                 |><| Time to live
                                                    |><| Protokoll (11 = UDP)
0000100 c4 95 c0 a8 01 9a ef 02 01 96 1b ec 1b ec 04 68
       |>   <| header checksum
             |>         <| source IP
                         |>         <| destination IP
                                     |>   <| UDP 
*/

#define MAX_HEAPS  16

typedef struct {
  uint32_t   magic;
  uint16_t   version_major;
  uint16_t   version_minor;
  uint32_t   time_zone_offset;
  uint32_t   timestamp_accuracy;
  uint32_t   snapshot_length;
  uint32_t   header_type;
} file_prefix_t;

typedef struct {
  uint32_t  timestamp_seconds;
  uint32_t  timestamp_nanoseconds;
  uint32_t  captured_length;
  uint32_t  untruncated_length;
} packet_prefix_t;

typedef struct {
  uint8_t   unknown[14];
  uint8_t   version_header_length;
  uint8_t   service_type;
  uint16_t  total_length;
  uint16_t  identification;
  uint16_t  flags_fragment_offset;
  uint8_t   time_to_live;
  uint8_t   protokoll;
  uint16_t  header_checksum;
  uint8_t   source_ip[4];
  uint8_t   destination_ip[4];
} ip_packet_header_t;

typedef struct {
  uint16_t  source_port;
  uint16_t  destination_port;
  uint16_t  total_length;
  uint16_t  checksum;
  size_t    available;
  size_t    used;
  uint8_t  *data;
} udp_packet_t;

static file_prefix_t        g_file_prefix;
static packet_prefix_t      g_packet_prefix;
static ip_packet_header_t   g_ip_packet_header;
static udp_packet_t         g_udp_packet;
static int                  g_heap_count;
static uint64_t             g_heap_id[MAX_HEAPS];
static uint64_t             g_heap_size[MAX_HEAPS];
static uint64_t             g_heap_remaining[MAX_HEAPS];

static void init_memory()
{
   g_udp_packet.available = 0;
   g_udp_packet.used = 0;
   g_udp_packet.data = NULL;
   g_heap_count = 0;
}

static uint8_t read_uint8(FILE *in)
{
  uint8_t   val;

  fread(&val, sizeof(val), 1, in);
  return val;
}

static uint16_t read_uint16(FILE *in)
{
  uint16_t  val;

  fread(&val, sizeof(val), 1, in);
  return (uint16_t)be16toh(val);
}

static uint32_t read_uint32(FILE *in)
{
  uint32_t  val;

  fread(&val, sizeof(val), 1, in);
  return (uint32_t)be32toh(val);
}

static uint64_t read_uint64(FILE *in)
{
  uint64_t  val;

  fread(&val, sizeof(val), 1, in);
  return (uint64_t)be64toh(val);
}

static void read_file_prefix(FILE *in, file_prefix_t *prefix)
{
  fread(&(prefix->magic), sizeof(prefix->magic), 1, in);
  fread(&(prefix->version_major), sizeof(prefix->version_major), 1, in);
  fread(&(prefix->version_minor), sizeof(prefix->version_minor), 1, in);
  fread(&(prefix->time_zone_offset), sizeof(prefix->time_zone_offset), 1, in);
  fread(&(prefix->timestamp_accuracy), sizeof(prefix->timestamp_accuracy), 1, in);
  fread(&(prefix->snapshot_length), sizeof(prefix->snapshot_length), 1, in);
  fread(&(prefix->header_type), sizeof(prefix->header_type), 1, in);
}

static void dump_file_prefix(file_prefix_t *prefix)
{
  printf("file magic = %d version = %d,%d snapshot_length = %d header type = %d\n",
    prefix->magic,
    prefix->version_major,
    prefix->version_minor,
    prefix->snapshot_length,
    prefix->header_type);
}

static void read_packet_prefix(FILE *in, packet_prefix_t *prefix)
{
  fread(&(prefix->timestamp_seconds), sizeof(prefix->timestamp_seconds), 1, in);
  fread(&(prefix->timestamp_nanoseconds), sizeof(prefix->timestamp_nanoseconds), 1, in);
  fread(&(prefix->captured_length), sizeof(prefix->captured_length), 1, in);
  fread(&(prefix->untruncated_length), sizeof(prefix->untruncated_length), 1, in);
}

static void dump_packet_prefix(packet_prefix_t *prefix)
{
  printf("packet timestamp %d, %d captured = %d untruncated = %d\n",
    prefix->timestamp_seconds,
    prefix->timestamp_nanoseconds,
    prefix->captured_length,
    prefix->untruncated_length);
}

static void read_ip_header(FILE *in, ip_packet_header_t *header)
{
  fread(&(header->unknown), sizeof(header->unknown), 1, in);
  header->version_header_length = read_uint8(in);
  header->service_type = read_uint8(in);
  header->total_length = read_uint16(in);
  header->identification = read_uint16(in);
  header->flags_fragment_offset = read_uint16(in);
  header->time_to_live = read_uint8(in);
  header->protokoll = read_uint8(in);
  header->header_checksum = read_uint16(in);
  fread(&(header->source_ip), sizeof(header->source_ip), 1, in);
  fread(&(header->destination_ip), sizeof(header->destination_ip), 1, in);
}

static void dump_ip_header(ip_packet_header_t *header)
{
  printf("ip total length %d protokoll %d source ip %d.%d.%d.%d destination ip %d.%d.%d.%d\n",
    header->total_length,
    header->protokoll,
    header->source_ip[0],
    header->source_ip[1],
    header->source_ip[2],
    header->source_ip[3],
    header->destination_ip[0],
    header->destination_ip[1],
    header->destination_ip[2],
    header->destination_ip[3]);
}

static void read_udp_packet(FILE *in, udp_packet_t *packet)
{
  packet->source_port = read_uint16(in);
  packet->destination_port = read_uint16(in);
  packet->total_length = read_uint16(in);
  packet->checksum = read_uint16(in);
  if (packet->total_length > packet->available) {
    packet->data = realloc(packet->data, (size_t)(packet->total_length));
    packet->available = packet->total_length;
  }
  packet->used = (size_t)(packet->total_length) - 4*sizeof(uint16_t);
  fread(packet->data, sizeof(uint8_t), packet->used, in);
}

static void dump_udp_packet(udp_packet_t *packet)
{
  printf("udp %d -> %d length %d\n",
    packet->source_port,
    packet->destination_port,
    packet->total_length);
}

static void dump_heap(udp_packet_t *packet)
{
  uint64_t   *spead_ptr = (uint64_t*)(packet->data);
  uint64_t    spead_header;
  int         nitems, iitem;
  uint64_t    heap_id = 0;
  uint64_t    heap_size = 0;
  uint64_t    heap_received = 0;
  int         index = -1;

  spead_header = (uint64_t)(be64toh(spead_ptr[0]));
  nitems = (int)(spead_header & 0xffff);
  printf("spead %d items", nitems);
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
    printf(" %d %04x %012lx", immediate_flag, item_tag, item_value);
    switch (item_tag) {
      case 1: heap_id = item_value; break;
      case 2: heap_size = item_value; break;
      case 4: heap_received = item_value; break;
      default:;
    }
  }
  for (index = 0; index < g_heap_count; index++) {
    if (g_heap_id[index] == heap_id) break;
  }
  if ((index == g_heap_count) && (index < MAX_HEAPS)) {
    g_heap_id[index] = heap_id;
    g_heap_size[index] = heap_size;
    g_heap_remaining[index] = heap_size;
    g_heap_count++;
  }
  g_heap_remaining[index] -= heap_received;
  printf("\n");
}

int main(int argc, char **args)
{
  FILE *in = NULL;
  int   index;

  init_memory();
  in = fopen(args[1], "r");
  if (in == NULL) {
    fprintf(stderr, "ERROR, cannot open file %s\n", args[1]);
    exit(1);
  }
  read_file_prefix(in, &g_file_prefix);
  dump_file_prefix(&g_file_prefix);
  while (!feof(in)) {
    read_packet_prefix(in, &g_packet_prefix);
    //dump_packet_prefix(&g_packet_prefix);
    read_ip_header(in, &g_ip_packet_header);
    //dump_ip_header(&g_ip_packet_header);
    if (g_ip_packet_header.protokoll != 0x11) {
      dump_ip_header(&g_ip_packet_header);
      fprintf(stdout, "ERROR, got a non UDP packet: %d\n", g_ip_packet_header.protokoll);
      break;
    } else {
      read_udp_packet(in, &g_udp_packet);
      //dump_udp_packet(&g_udp_packet);
      dump_heap(&g_udp_packet);
    }
  }
  fclose(in);
  for (index = 0; index < g_heap_count; index++) {
    printf("heap %ld size %ld remaining %ld\n",
      g_heap_id[index],
      g_heap_size[index],
      g_heap_remaining[index]);
  }
  return 0;
}

