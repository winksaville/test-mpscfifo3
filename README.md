test mpscfifo2
===

This combines a MspcRingBuffer and an MpscLinkList into an MpscFifo.
The goal was that it would be the performance of the ring buffer when
the fifo was mostly empty and no slower when there were large numbers
of items were on the list.

I failed, its significantly slower. I see 44.2ns/op when using perf
for the "empty" case compared to 14ns/op mpscfifo2 and 11.5ns for
mpscringbuff.
```
wink@wink-desktop:~/prgs/test-mpscfifo3 (master)
$ make clean ; make CC=clang ; ./simple 100000000
clang -Wall -std=c11 -O2 -g -pthread -c test.c -o test.o
clang -Wall -std=c11 -O2 -g -pthread -c mpscfifo.c -o mpscfifo.o
clang -Wall -std=c11 -O2 -g -pthread -c mpscringbuff.c -o mpscringbuff.o
clang -Wall -std=c11 -O2 -g -pthread -c mpsclinklist.c -o mpsclinklist.o
clang -Wall -std=c11 -O2 -g -pthread -c msg_pool.c -o msg_pool.o
clang -Wall -std=c11 -O2 -g -pthread -c diff_timespec.c -o diff_timespec.o
clang -Wall -std=c11 -O2 -g -pthread test.o mpscfifo.o mpscringbuff.o mpsclinklist.o msg_pool.o diff_timespec.o -o test
objdump -d test > test.txt
clang -Wall -std=c11 -O2 -g -pthread -c simple.c -o simple.o
clang -Wall -std=c11 -O2 -g -pthread simple.o mpscfifo.o mpscringbuff.o mpsclinklist.o msg_pool.o diff_timespec.o -o simple
objdump -d simple > simple.txt
test loops=100000000
     1 7f418cd22700  simple:+
     2 7f418cd22700  simple: init cmdFifo=0x7ffef2321c00
     3 7f418cd22700  simple: remove from empty cmdFifo=0x7ffef2321c00
     4 7f418cd22700  simple: add a message to empty cmdFifo=0x7ffef2321c00
     5 7f418cd22700  simple: remove from with one item in cmdFifo=0x7ffef2321c00
     6 7f418cd22700  simple: add msg1 to empty cmdFifo=0x7ffef2321c00
     7 7f418cd22700  simple: add msg2 to non-empty cmdFifo=0x7ffef2321c00
     8 7f418cd22700  simple: add msg3 to non-empty cmdFifo=0x7ffef2321c00
     9 7f418cd22700  simple: remove msg1 from cmdFifo=0x7ffef2321c00
    10 7f418cd22700  simple: remove msg2 from cmdFifo=0x7ffef2321c00
    11 7f418cd22700  simple: remove msg3 from cmdFifo=0x7ffef2321c00
    12 7f418cd22700  simple: remove from empty cmdFifo=0x7ffef2321c00
    13 7f418cd22700  simple:-error=0

    14 7f418cd22700  perf:+loops=100000000
    15 7f418cd22700  perf: init cmdFifo=0x7ffef2321bc0
    16 7f418cd22700  perf: remove from empty cmdFifo=0x7ffef2321bc0
    17 7f418cd22700  perf: add_rmv from empty fifo  processing=4.470s
    18 7f418cd22700  perf: add rmv from empty fifo ops_per_sec=22373080.984
    19 7f418cd22700  perf: add rmv from empty fifo   ns_per_op=44.7ns
    20 7f418cd22700  perf: add_rmv from non-empty fifo  processing=8.843s
    21 7f418cd22700  perf: add rmv from non-empty fifo ops_per_sec=22616815.818
    22 7f418cd22700  perf: add rmv from non-empty fifo   ns_per_op=44.2ns
    23 7f418cd22700  perf:-error=0

Success

wink@wink-desktop:~/prgs/test-mpscfifo2 (master)
$ ./simple 1000000000
test loops=1000000000
     1 7ff016aff700  simple:+
     2 7ff016aff700  simple: init cmdFifo=0x7ffe5d09ab40
     3 7ff016aff700  simple: remove from empty cmdFifo=0x7ffe5d09ab40
     4 7ff016aff700  simple: add a message to empty cmdFifo=0x7ffe5d09ab40
     5 7ff016aff700  simple: remove from with one item in cmdFifo=0x7ffe5d09ab40
     6 7ff016aff700  simple: add a message to empty cmdFifo=0x7ffe5d09ab40
     7 7ff016aff700  simple: add a message to non-empty cmdFifo=0x7ffe5d09ab40
     8 7ff016aff700  simple: remove msg1 from cmdFifo=0x7ffe5d09ab40
     9 7ff016aff700  simple: remove msg2 from cmdFifo=0x7ffe5d09ab40
    10 7ff016aff700  simple: remove from empty cmdFifo=0x7ffe5d09ab40
    11 7ff016aff700  simple:-error=0

    12 7ff016aff700  perf:+loops=1000000000
    13 7ff016aff700  perf: init cmdFifo=0x7ffe5d09ab40
    14 7ff016aff700  perf: remove from empty cmdFifo=0x7ffe5d09ab40
    15 7ff016aff700  perf: add_rmv from empty fifo  processing=14.103s
    16 7ff016aff700  perf: add rmv from empty fifo ops_per_sec=70905708.359
    17 7ff016aff700  perf: add rmv from empty fifo   ns_per_op=14.1ns
    18 7ff016aff700  perf: add_rmv from non-empty fifo  processing=13.945s
    19 7ff016aff700  perf: add rmv from non-empty fifo ops_per_sec=71707760.637
    20 7ff016aff700  perf: add rmv from non-empty fifo   ns_per_op=13.9ns
    21 7ff016aff700  perf:-error=0

Success

wink@wink-desktop:~/prgs/test-mpscringbuff (master)
$ ./simple 1000000000
test loops=1000000000
     1 7fa8294e1700  simple:+
     2 7fa8294e1700  simple: init pool=0x7fff95750ac0
     3 7fa8294e1700  simple: init cmdFifo=0x7fff95750b80
     4 7fa8294e1700  simple: remove from empty cmdFifo=0x7fff95750b80
     5 7fa8294e1700  simple: add a message to empty cmdFifo=0x7fff95750b80
     6 7fa8294e1700  simple: remove from with one item in cmdFifo=0x7fff95750b80
     7 7fa8294e1700  simple: add a message to empty cmdFifo=0x7fff95750b80
     8 7fa8294e1700  simple: add a message to non-empty cmdFifo=0x7fff95750b80
     9 7fa8294e1700  simple: remove msg1 from cmdFifo=0x7fff95750b80
    10 7fa8294e1700  simple: remove msg2 from cmdFifo=0x7fff95750b80
    11 7fa8294e1700  simple: remove from empty cmdFifo=0x7fff95750b80
    12 7fa8294e1700  simple:-error=0

    13 7fa8294e1700  perf:+loops=1000000000
    14 7fa8294e1700  simple: init pool=0x7fff95750a80
    15 7fa8294e1700  perf: add_rmv from empty fifo  processing=11.553s
    16 7fa8294e1700  perf: add rmv from empty fifo ops_per_sec=86560817.348
    17 7fa8294e1700  perf: add rmv from empty fifo   ns_per_op=11.6ns
    18 7fa8294e1700  perf: add_rmv from non-empty fifo  processing=11.381s
    19 7fa8294e1700  perf: add rmv from non-empty fifo ops_per_sec=87865889.873
    20 7fa8294e1700  perf: add rmv from non-empty fifo   ns_per_op=11.4ns
    21 7fa8294e1700  perf:-error=0

Success
```

When using the test app it wasn't quite as bad but still bad. We see
42.3ns/op for this, 21.4ns/op for mpscfifo2 and 17.9ns/op for
the ringbuffer:
```
wink@wink-desktop:~/prgs/test-mpscfifo3 (master)
$ make clean ; make CC=clang ; ./test 12 10000000 0x100000
clang -Wall -std=c11 -O2 -g -pthread -c test.c -o test.o
clang -Wall -std=c11 -O2 -g -pthread -c mpscfifo.c -o mpscfifo.o
clang -Wall -std=c11 -O2 -g -pthread -c mpscringbuff.c -o mpscringbuff.o
clang -Wall -std=c11 -O2 -g -pthread -c mpsclinklist.c -o mpsclinklist.o
clang -Wall -std=c11 -O2 -g -pthread -c msg_pool.c -o msg_pool.o
clang -Wall -std=c11 -O2 -g -pthread -c diff_timespec.c -o diff_timespec.o
clang -Wall -std=c11 -O2 -g -pthread test.o mpscfifo.o mpscringbuff.o mpsclinklist.o msg_pool.o diff_timespec.o -o test
objdump -d test > test.txt
clang -Wall -std=c11 -O2 -g -pthread -c simple.c -o simple.o
clang -Wall -std=c11 -O2 -g -pthread simple.o mpscfifo.o mpscringbuff.o mpsclinklist.o msg_pool.o diff_timespec.o -o simple
objdump -d simple > simple.txt
test client_count=12 loops=10000000 msg_count=1048576
     1 7f6cb02ac700  multi_thread_msg:+client_count=12 loops=10000000 msg_count=1048576
     2 7f6cb02ac700  multi_thread_msg: cmds_processed=534483164 msgs_processed=1082597972 mt_msgs_sent=45360012 mt_no_msgs=74639988
     3 7f6cb02ac700  startup=1.194188
     4 7f6cb02ac700  looping=41.247790
     5 7f6cb02ac700  disconnecting=4.303393
     6 7f6cb02ac700  stopping=0.029284
     7 7f6cb02ac700  complete=0.240067
     8 7f6cb02ac700  processing=45.821s
     9 7f6cb02ac700  cmds_per_sec=11664708.051
    10 7f6cb02ac700  ns_per_cmd=85.7ns
    11 7f6cb02ac700  msgs_per_sec=23626916.863
    12 7f6cb02ac700  ns_per_msg=42.3ns
    13 7f6cb02ac700  total=47.015
    14 7f6cb02ac700  multi_thread_msg:-error=0

Success
wink@wink-desktop:~/prgs/test-mpscfifo3 (wip-mpsclinklist)
$ cd ../test-mpscfifo2
wink@wink-desktop:~/prgs/test-mpscfifo2 (master)
$ make ; ./test 12 10000000 0x100000
make: Nothing to be done for 'all'.
test client_count=12 loops=10000000 msg_count=1048576
     1 7f9e6c14d700  multi_thread_msg:+client_count=12 loops=10000000 msg_count=1048576
     2 7f9e6c14d700  multi_thread_msg: cmds_processed=868624958 msgs_processed=1750881572 mt_msgs_sent=74168110 mt_no_msgs=45831890
     3 7f9e6c14d700  startup=0.464311
     4 7f9e6c14d700  looping=30.884191
     5 7f9e6c14d700  disconnecting=6.409298
     6 7f9e6c14d700  stopping=0.001032
     7 7f9e6c14d700  complete=0.225598
     8 7f9e6c14d700  processing=37.520s
     9 7f9e6c14d700  cmds_per_sec=23150911.886
    10 7f9e6c14d700  ns_per_cmd=43.2ns
    11 7f9e6c14d700  msgs_per_sec=46665139.682
    12 7f9e6c14d700  ns_per_msg=21.4ns
    13 7f9e6c14d700  total=37.984
    14 7f9e6c14d700  multi_thread_msg:-error=0

Success
wink@wink-desktop:~/prgs/test-mpscfifo2 (master)
$ cd ../test-mpscringbuff/
wink@wink-desktop:~/prgs/test-mpscringbuff (master)
$ make ; ./test 12 10000000 0x100000
make: Nothing to be done for 'all'.
test client_count=12 loops=10000000 msg_count=1048576
     1 7fb6be695700  multi_thread_msg:+client_count=12 loops=10000000 msg_count=1048576
     2 7fb6be695700  multi_thread_msg: cmds_processed=565512864 msgs_processed=1518832808 no_msgs=32376 rbuf_full=370167074 mt_msgs_sent=77995730  mt_no_msgs=42004270
     3 7fb6be695700  startup=0.542307
     4 7fb6be695700  looping=26.523650
     5 7fb6be695700  disconneting=0.670620
     6 7fb6be695700  stopping=0.005471
     7 7fb6be695700  complete=0.015336
     8 7fb6be695700  processing=27.215s
     9 7fb6be695700  cmds_per_sec=20779395.585
    10 7fb6be695700  ns_per_cmd=48.1ns
    11 7fb6be695700  msgs_per_sec=55808505.437
    12 7fb6be695700  ns_per_msg=17.9ns
    13 7fb6be695700  total=27.757
    14 7fb6be695700  multi_thread_msg:-error=0

Success
```

It should be noted that gcc is slightly faster than clang,
40.5ns vs 44.2ns for perf and the same seed for test 42.1ns vs 42.3ns.
```
wink@wink-desktop:~/prgs/test-mpscfifo3 (master)
$ make clean ; make CC=gcc ; ./simple 100000000
gcc -Wall -std=c11 -O2 -g -pthread -c test.c -o test.o
gcc -Wall -std=c11 -O2 -g -pthread -c mpscfifo.c -o mpscfifo.o
gcc -Wall -std=c11 -O2 -g -pthread -c mpscringbuff.c -o mpscringbuff.o
gcc -Wall -std=c11 -O2 -g -pthread -c mpsclinklist.c -o mpsclinklist.o
gcc -Wall -std=c11 -O2 -g -pthread -c msg_pool.c -o msg_pool.o
gcc -Wall -std=c11 -O2 -g -pthread -c diff_timespec.c -o diff_timespec.o
gcc -Wall -std=c11 -O2 -g -pthread test.o mpscfifo.o mpscringbuff.o mpsclinklist.o msg_pool.o diff_timespec.o -o test
objdump -d test > test.txt
gcc -Wall -std=c11 -O2 -g -pthread -c simple.c -o simple.o
gcc -Wall -std=c11 -O2 -g -pthread simple.o mpscfifo.o mpscringbuff.o mpsclinklist.o msg_pool.o diff_timespec.o -o simple
objdump -d simple > simple.txt
test loops=100000000
     1 7f5e8eb10700  simple:+
     2 7f5e8eb10700  simple: init cmdFifo=0x7ffda6fd4440
     3 7f5e8eb10700  simple: remove from empty cmdFifo=0x7ffda6fd4440
     4 7f5e8eb10700  simple: add a message to empty cmdFifo=0x7ffda6fd4440
     5 7f5e8eb10700  simple: remove from with one item in cmdFifo=0x7ffda6fd4440
     6 7f5e8eb10700  simple: add msg1 to empty cmdFifo=0x7ffda6fd4440
     7 7f5e8eb10700  simple: add msg2 to non-empty cmdFifo=0x7ffda6fd4440
     8 7f5e8eb10700  simple: add msg3 to non-empty cmdFifo=0x7ffda6fd4440
     9 7f5e8eb10700  simple: remove msg1 from cmdFifo=0x7ffda6fd4440
    10 7f5e8eb10700  simple: remove msg2 from cmdFifo=0x7ffda6fd4440
    11 7f5e8eb10700  simple: remove msg3 from cmdFifo=0x7ffda6fd4440
    12 7f5e8eb10700  simple: remove from empty cmdFifo=0x7ffda6fd4440
    13 7f5e8eb10700  simple:-error=0

    14 7f5e8eb10700  perf:+loops=100000000
    15 7f5e8eb10700  perf: init cmdFifo=0x7ffda6fd4440
    16 7f5e8eb10700  perf: remove from empty cmdFifo=0x7ffda6fd4440
    17 7f5e8eb10700  perf: add_rmv from empty fifo  processing=4.077s
    18 7f5e8eb10700  perf: add rmv from empty fifo ops_per_sec=24525968.362
    19 7f5e8eb10700  perf: add rmv from empty fifo   ns_per_op=40.8ns
    20 7f5e8eb10700  perf: add_rmv from non-empty fifo  processing=8.092s
    21 7f5e8eb10700  perf: add rmv from non-empty fifo ops_per_sec=24716679.622
    22 7f5e8eb10700  perf: add rmv from non-empty fifo   ns_per_op=40.5ns
    23 7f5e8eb10700  perf:-error=0

Success
wink@wink-desktop:~/prgs/test-mpscfifo3 (master)
$ make clean ; make CC=gcc ; ./test 12 10000000 0x100000
gcc -Wall -std=c11 -O2 -g -pthread -c test.c -o test.o
gcc -Wall -std=c11 -O2 -g -pthread -c mpscfifo.c -o mpscfifo.o
gcc -Wall -std=c11 -O2 -g -pthread -c mpscringbuff.c -o mpscringbuff.o
gcc -Wall -std=c11 -O2 -g -pthread -c mpsclinklist.c -o mpsclinklist.o
gcc -Wall -std=c11 -O2 -g -pthread -c msg_pool.c -o msg_pool.o
gcc -Wall -std=c11 -O2 -g -pthread -c diff_timespec.c -o diff_timespec.o
gcc -Wall -std=c11 -O2 -g -pthread test.o mpscfifo.o mpscringbuff.o mpsclinklist.o msg_pool.o diff_timespec.o -o test
objdump -d test > test.txt
gcc -Wall -std=c11 -O2 -g -pthread -c simple.c -o simple.o
gcc -Wall -std=c11 -O2 -g -pthread simple.o mpscfifo.o mpscringbuff.o mpsclinklist.o msg_pool.o diff_timespec.o -o simple
objdump -d simple > simple.txt
test client_count=12 loops=10000000 msg_count=1048576
     1 7fce4bfb9700  multi_thread_msg:+client_count=12 loops=10000000 msg_count=1048576
     2 7fce4bfb9700  multi_thread_msg: cmds_processed=520599457 msgs_processed=1054830558 mt_msgs_sent=44200694 mt_no_msgs=75799306
     3 7fce4bfb9700  startup=1.145295
     4 7fce4bfb9700  looping=40.488684
     5 7fce4bfb9700  disconnecting=3.610894
     6 7fce4bfb9700  stopping=0.012393
     7 7fce4bfb9700  complete=0.270340
     8 7fce4bfb9700  processing=44.382s
     9 7fce4bfb9700  cmds_per_sec=11729886.013
    10 7fce4bfb9700  ns_per_cmd=85.3ns
    11 7fce4bfb9700  msgs_per_sec=23766913.396
    12 7fce4bfb9700  ns_per_msg=42.1ns
    13 7fce4bfb9700  total=45.528
    14 7fce4bfb9700  multi_thread_msg:-error=0

Success
```
