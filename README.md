Receive process for getting SPEAD heaps from 1 40 GBit connection
and put them into a PSRDADA ringbuffer.

create a dada ringbuffer which is large enough for testing purposes:

dada_db -d dada
dada_db -k dada -n 64 -b 67108864 -p

create a process which gets data out of a ringbuffer:

../psrdada_cpp/psrdada_cp/dbnull > /dev/null &

use the standard linux socket implementation to get data from a multicast group:

mkrecv --joint --threads 64 --heaps 64 \
--freq_first 0 --freq_step 256 --freq_count 1 \
--feng_first 7 --feng_count 1 \
--time_step 2097152 \
--port 7148 --udp_if 192.168.1.20 \
239.2.1.150

the multicast groups goe from 150 up to 165 (16 groups)

use the infiniband library to get data from a multicast group:

mkrecv --joint --threads 4 --heaps 16 \
--freq_first 0 --freq_step 256 --freq_count 1 \
--feng_first 7 --feng_count 1 \
--time_step 2097152 \
--port 7148 --ibv 192.168.1.20 --ibv-vector -1 --ibv-max-poll 1000 \
239.2.1.150

a small test program does exist to use the standard socket implementation to get data from several multicast groups:

./recv_heaps -quiet 192.168.1.20 239.2.1.150 239.2.1.151


