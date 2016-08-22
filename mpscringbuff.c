/**
 * This software is released into the public domain.
 *
 * A MpscRingBuff is a wait free/thread safe multi-producer
 * single consumer ring buffer.
 *
 * The ring buffer has a head and tail, the elements are added
 * to the head removed from the tail.
 */

#define NDEBUG

#define _DEFAULT_SOURCE

#define DELAY 0

#ifndef NDEBUG
#define COUNT
#endif

#include "msg.h"
#include "mpscfifo.h"
#include "mpscringbuff.h"
#include "dpf.h"

#include <sys/types.h>
#include <pthread.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <memory.h>


/**
 * @see mpscringbuff.h
 */
MpscRingBuff_t* rb_init(MpscRingBuff_t* pRb, uint32_t size) {
  DPF(LDR "initMpscRingBuff:+pRb=%p size=%d\n", ldr(), pRb, size);
  pRb->add_idx = 0;
  pRb->rmv_idx = 0;
  pRb->size = size;
  pRb->mask = size - 1;
  pRb->msgs_processed = 0;
  if ((size & pRb->mask) != 0) {
    printf(LDR "initMpscRingBuff:-pRb=%p size=%d not power of 2 return NULL\n", ldr(), pRb, size);
    return NULL;
  }
  pRb->count = 0;
  pRb->empty = 0;
  pRb->ring_buffer = malloc(size * sizeof(pRb->ring_buffer[0]));
  for (uint32_t i = 0; i < pRb->size; i++) {
    pRb->ring_buffer[i].seq = i;
    pRb->ring_buffer[i].pMsg = NULL;
  }
  if (pRb->ring_buffer == NULL) {
    printf(LDR "initMpscRingBuff:-pRb=%p size=%d could not allocate ring_buffer return NULL\n", ldr(), pRb, size);
    return NULL;
  }
  DPF(LDR "initMpscRingBuff:-pRb=%p size=%d\n", ldr(), pRb, size);
  return pRb;
}

/**
 * @see mpscringbuff.h
 */
uint64_t rb_deinit(MpscRingBuff_t* pRb) {
  DPF(LDR "deinitMpscRingBuff:+pRb=%p\n", ldr(), pRb);
  uint64_t msgs_processed = pRb->msgs_processed;
  free(pRb->ring_buffer);
  pRb->ring_buffer = NULL;
  pRb->add_idx = 0;
  pRb->rmv_idx = 0;
  pRb->size = 0;
  pRb->mask = 0;
  pRb->count = 0;
  pRb->msgs_processed = 0;
  DPF(LDR "deinitMpscRingBuff:-pRb=%p msgs_processed=%lu\n", ldr(), pRb, msgs_processed);
  return msgs_processed;
}

/**
 * @see mpscringbuff.h
 */
bool rb_add(MpscRingBuff_t* pRb, Msg_t* pMsg) {
  Cell_t* cell;
  uint32_t pos = pRb->add_idx;

  while (true) {
    cell = &pRb->ring_buffer[pos & pRb->mask];
    uint32_t seq = __atomic_load_n(&cell->seq, __ATOMIC_ACQUIRE);
    int32_t dif = seq - pos;

    if (dif == 0) {
      if (__atomic_compare_exchange_n((uint32_t*)&pRb->add_idx, &pos, pos + 1, true, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        break;
      }
    } else if (dif < 0) {
      return false;
    } else {
      pos = pRb->add_idx;
    }
  }

  pRb->count += 1;
  cell->pMsg = pMsg;
  __atomic_store_n(&cell->seq, pos + 1, __ATOMIC_RELEASE);

  return true;
}

/**
 * @see mpscringbuff.h
 */
Msg_t* rb_rmv(MpscRingBuff_t* pRb) {
  Msg_t* pMsg;
  Cell_t* cell;
  uint32_t pos = pRb->rmv_idx;

  cell = &pRb->ring_buffer[pos & pRb->mask];
  uint32_t seq = __atomic_load_n(&cell->seq, __ATOMIC_ACQUIRE);
  int32_t dif = seq - (pos + 1);

  if (dif < 0) {
    if (++pRb->empty > 1) {
      //printf(LDR "rb_rmv: EMPTY dif=%d count=%d empty=%d seq=%d pos=%d\n",
      //    ldr(), dif, pRb->count, pRb->empty, seq, pos);
      //CRASH();
    }
    return NULL;
  }
  
  if (dif > 0) CRASH();

  pRb->rmv_idx += 1;
  pRb->count -= 1;
  pRb->msgs_processed += 1;

  pMsg = cell->pMsg;
  __atomic_store_n(&cell->seq, pos + pRb->mask + 1, __ATOMIC_RELEASE);

  return pMsg;
}
