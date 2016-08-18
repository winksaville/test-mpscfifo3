/**
 * This software is released into the public domain.
 */

#define NDEBUG

#define _DEFAULT_SOURCE

#include "mpscfifo.h"
#include "diff_timespec.h"
#include "dpf.h"

#include <sys/types.h>
#include <pthread.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * We pass pointers in Msg_t.arg2 which is a uint64_t,
 * verify a void* fits.
 * TODO: sizeof(uint64_t) should be sizeof(Msg_t.arg2), how to do that?
 */
_Static_assert(sizeof(uint64_t) >= sizeof(void*), "Expect sizeof uint64_t >= sizeof void*");

_Atomic(uint64_t) gTick = 0;

bool simple(void) {
  bool error = false;
  MpscFifo_t cmdFifo;

  printf(LDR "simple:+\n", ldr());

  Cell_t cell1;
  Cell_t cell2;

  Msg_t msg1 = {
    .pCell = &cell1,
    .pPool = NULL,
    .arg1 = 1,
    .arg2 = -1 
  };
  
  Msg_t msg2 = {
    .pCell = &cell2,
    .pPool = NULL,
    .arg1 = 2,
    .arg2 = -2
  };
  
  printf(LDR "simple: init cmdFifo=%p\n", ldr(), &cmdFifo);
  initMpscFifo(&cmdFifo);

  printf(LDR "simple: remove from empty cmdFifo=%p\n", ldr(), &cmdFifo);
  Msg_t* pMsg = rmv(&cmdFifo);
  if (pMsg != NULL) {
    printf(LDR "simple: expected pMsg=%p == NULL\n", ldr(), pMsg);
    error |= true;
  }
  
  printf(LDR "simple: add a message to empty cmdFifo=%p\n", ldr(), &cmdFifo);
  add(&cmdFifo, &msg1);

  printf(LDR "simple: remove from with one item in cmdFifo=%p\n", ldr(), &cmdFifo);
  pMsg = rmv(&cmdFifo);
  if (pMsg == NULL) {
    printf(LDR "simple: expected pMsg=%p != NULL\n", ldr(), pMsg);
    error |= true;
  }
  
  printf(LDR "simple: add a message to empty cmdFifo=%p\n", ldr(), &cmdFifo);
  add(&cmdFifo, &msg1);

  printf(LDR "simple: add a message to non-empty cmdFifo=%p\n", ldr(), &cmdFifo);
  add(&cmdFifo, &msg2);


  printf(LDR "simple: remove msg1 from cmdFifo=%p\n", ldr(), &cmdFifo);
  pMsg = rmv(&cmdFifo);
  if (pMsg == NULL) {
    printf(LDR "simple: expected pMsg=%p != NULL\n", ldr(), pMsg);
    error |= true;
  } else if (pMsg != &msg1) {
    printf(LDR "simple: expected pMsg=%p == &msg1=%p\n", ldr(), pMsg, &msg1);
    error |= true;
  }
  
  printf(LDR "simple: remove msg2 from cmdFifo=%p\n", ldr(), &cmdFifo);
  pMsg = rmv(&cmdFifo);
  if (pMsg == NULL) {
    printf(LDR "simple: expected pMsg=%p != NULL\n", ldr(), pMsg);
    error |= true;
  } else if (pMsg != &msg2) {
    printf(LDR "simple: expected pMsg=%p == &msg2=%p\n", ldr(), pMsg, &msg2);
    error |= true;
  }
  
  printf(LDR "simple: remove from empty cmdFifo=%p\n", ldr(), &cmdFifo);
  pMsg = rmv(&cmdFifo);
  if (pMsg != NULL) {
    printf(LDR "simple: expected pMsg=%p == NULL\n", ldr(), pMsg);
    error |= true;
  }
  
  printf(LDR "simple:-error=%u\n\n", ldr(), error);

  return error;
}

bool perf(const uint64_t loops) {
  bool error = false;
  struct timespec time_start;
  struct timespec time_stop;
  MpscFifo_t cmdFifo;

  printf(LDR "perf:+loops=%lu\n", ldr(), loops);

  Cell_t cell1;
  Cell_t cell2;

  Msg_t msg1 = {
    .pCell = &cell1,
    .pPool = NULL,
    .arg1 = 1,
    .arg2 = -1 
  };
  
  Msg_t msg2 = {
    .pCell = &cell2,
    .pPool = NULL,
    .arg1 = 2,
    .arg2 = -2
  };
  
  printf(LDR "perf: init cmdFifo=%p\n", ldr(), &cmdFifo);
  initMpscFifo(&cmdFifo);

  printf(LDR "perf: remove from empty cmdFifo=%p\n", ldr(), &cmdFifo);
  Msg_t* pMsg = rmv(&cmdFifo);
  if (pMsg != NULL) {
    printf(LDR "perf: expected pMsg=%p == NULL\n", ldr(), pMsg);
    error |= true;
  }

  clock_gettime(CLOCK_REALTIME, &time_start);
  for (uint64_t i = 0; i < loops; i++) {
    add(&cmdFifo, &msg1);
    rmv(&cmdFifo);
  }
  clock_gettime(CLOCK_REALTIME, &time_stop);
  
  double processing_ns = diff_timespec_ns(&time_stop, &time_start);
  printf(LDR "perf: add_rmv from empty fifo  processing=%.3fs\n", ldr(), processing_ns / ns_flt);
  double ops_per_sec = (loops * ns_flt) / processing_ns;
  printf(LDR "perf: add rmv from empty fifo ops_per_sec=%.3f\n", ldr(), ops_per_sec);
  double ns_per_op = (float)processing_ns / (double)loops;
  printf(LDR "perf: add rmv from empty fifo   ns_per_op=%.1fns\n", ldr(), ns_per_op);

  add(&cmdFifo, &msg2);

  clock_gettime(CLOCK_REALTIME, &time_start);
  for (uint64_t i = 0; i < loops; i++) {
    add(&cmdFifo, &msg1);
    rmv(&cmdFifo);
  }
  clock_gettime(CLOCK_REALTIME, &time_stop);
  
  processing_ns = diff_timespec_ns(&time_stop, &time_start);
  printf(LDR "perf: add_rmv from non-empty fifo  processing=%.3fs\n", ldr(), processing_ns / ns_flt);
  ops_per_sec = (loops * ns_flt) / processing_ns;
  printf(LDR "perf: add rmv from non-empty fifo ops_per_sec=%.3f\n", ldr(), ops_per_sec);
  ns_per_op = (float)processing_ns / (double)loops;
  printf(LDR "perf: add rmv from non-empty fifo   ns_per_op=%.1fns\n", ldr(), ns_per_op);
  printf(LDR "perf:-error=%u\n\n", ldr(), error);

  return error;
}

int main(int argc, char* argv[]) {
  bool error = false;

  if (argc != 2) {
    printf("Usage:\n");
    printf(" %s loops\n", argv[0]);
    return 1;
  }

  u_int64_t loops;
  sscanf(argv[1], "%lu", &loops);
  printf("test loops=%lu\n", loops);

  error |= simple();
  error |= perf(loops);

  if (!error) {
    printf("Success\n");
  }

  return error ? 1 : 0;
}
