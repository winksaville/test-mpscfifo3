/*
 * This software is released into the public domain.
 *
 * A MpscFifo is a wait free/thread safe multi-producer
 * single consumer first in first out queue. This algorithm
 * is from Dimitry Vyukov's non intrusive MPSC code here:
 *   http://www.1024cores.net/home/lock-free-algorithms/queues/non-intrusive-mpsc-node-based-queue
 *
 * The fifo has a head and tail, the elements are added
 * to the head of the queue and removed from the tail.
 * To allow for a wait free algorithm a stub element is used
 * so that a single atomic instruction can be used to add and
 * remove an element. Therefore, when you create a queue you
 * must pass in an areana which is used to manage the stub.
 *
 * A consequence of this algorithm is that when you add an
 * element to the queue a different element is returned when
 * you remove it from the queue. Of course the contents are
 * the same but the returned pointer will be different.
 */

#define NDEBUG

#define _DEFAULT_SOURCE

#define DELAY 0

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

/**
 * @see mpscfifo.h
 */
MpscFifo_t *initMpscFifo(MpscFifo_t *pQ) {
  DPF(LDR "initMpscFifo:*pQ=%p\n", ldr(), pQ);
  pQ->cell.pNext = NULL;
  pQ->cell.pMsg = NULL;
  pQ->pHead = &pQ->cell;
  pQ->pTail = &pQ->cell;
  pQ->count = 0;
  pQ->msgs_processed = 0;
  pQ->use_rb = true;
  rb_init(&pQ->rb, 256);
  return pQ;
}

/**
 * @see mpscfifo.h
 */
uint64_t deinitMpscFifo(MpscFifo_t *pQ) {
  uint64_t msgs_processed = pQ->msgs_processed;
  msgs_processed += rb_deinit(&pQ->rb);
  pQ->use_rb = false;
#ifndef NDEBUG
  uint32_t count = pQ->count;
#endif
  Cell_t *pStub = pQ->pHead;
  pStub->pNext = NULL;
  pQ->pHead = NULL;
  pQ->pTail = NULL;
  pQ->count = 0;
  pQ->msgs_processed = 0;

  DPF(LDR "deinitMpscFifo:-pQ=%p count=%u msgs_processed=%lu\n", ldr(), pQ, count, msgs_processed);
  return msgs_processed;
}

/**
 * @see mpscifo.h
 */
void add(MpscFifo_t *pQ, Msg_t *pMsg) {
  if (__atomic_load_n(&pQ->use_rb, __ATOMIC_ACQUIRE)) {
    if (rb_add(&pQ->rb, pMsg)) {
      return;
    }
    //printf("add: set use_rb=false\n");
    __atomic_store_n(&pQ->use_rb, false, __ATOMIC_RELEASE);
  }
  Cell_t* pCell = pMsg->pCell;
  pCell->pNext = NULL;
  pCell->pMsg = pMsg;
  Cell_t* pPrev = __atomic_exchange_n(&pQ->pHead, pCell, __ATOMIC_ACQ_REL);
  // rmv will stall spinning if preempted at this critical spot
  __atomic_store_n(&pPrev->pNext, pCell, __ATOMIC_RELEASE);
}

/**
 * @see mpscifo.h
 */
Msg_t *rmv_non_stalling(MpscFifo_t *pQ) {
  Msg_t* pMsg;

  if (__atomic_load_n(&pQ->use_rb, __ATOMIC_ACQUIRE)) {
    pMsg = rb_rmv(&pQ->rb);
    return pMsg;
  }
  Cell_t* pTail = pQ->pTail;
  Cell_t* pNext = pTail->pNext;
  if (pNext != NULL) {
    pMsg = pNext->pMsg;
    pMsg->pCell = pTail;
    pQ->pTail = pNext;
    pQ->msgs_processed += 1;
  } else {
    pMsg = rb_rmv(&pQ->rb);
    assert(pMsg != NULL);
    pQ->use_rb = true;
  }
  return pMsg;
}

/**
 * @see mpscifo.h
 */
Msg_t *rmv(MpscFifo_t *pQ) {
  Msg_t* pMsg;

  if (__atomic_load_n(&pQ->use_rb, __ATOMIC_ACQUIRE)) {
    pMsg = rb_rmv(&pQ->rb);
    return pMsg;
  }
  Cell_t* pTail = pQ->pTail;
  Cell_t* pNext = pTail->pNext;
  if ((pNext == NULL) && (pTail == __atomic_load_n(&pQ->pHead, __ATOMIC_ACQUIRE))) {
    pMsg = rb_rmv(&pQ->rb);
    //printf("add: set use_rb=true\n");
    assert(pMsg != NULL);
    __atomic_store_n(&pQ->use_rb, true, __ATOMIC_RELEASE);
    return pMsg;
  } else {
    if (pNext == NULL) {
      while ((pNext = __atomic_load_n(&pTail->pNext, __ATOMIC_ACQUIRE)) == NULL) {
        sched_yield();
      }
    }
    pMsg = pNext->pMsg;
    pMsg->pCell = pTail;
    pQ->pTail = pNext;
    pQ->msgs_processed += 1;
    return pMsg;
  }
}

/**
 * @see mpscfifo.h
 */
void ret_msg(Msg_t* pMsg) {
  if ((pMsg != NULL) && (pMsg->pPool != NULL)) {
    DPF(LDR "ret_msg: pool=%p msg=%p arg1=%lu arg2=%lu\n", ldr(), pMsg->pPool, pMsg, pMsg->arg1, pMsg->arg2);
    add(pMsg->pPool, pMsg);
  } else {
    if (pMsg == NULL) {
      DPF(LDR "ret:#No msg msg=%p\n", ldr(), pMsg);
    } else {
      DPF(LDR "ret:#No pool msg=%p pool=%p arg1=%lu arg2=%lu\n",
          ldr(), pMsg, pMsg->pPool, pMsg->arg1, pMsg->arg2);
    }
  }
}

/**
 * @see mpscfifo.h
 */
void send_rsp_or_ret(Msg_t* msg, uint64_t arg1) {
  if (msg->pRspQ != NULL) {
    MpscFifo_t* pRspQ = msg->pRspQ;
    msg->pRspQ = NULL;
    msg->arg1 = arg1;
    DPF(LDR "send_rsp_or_ret: send pRspQ=%p msg=%p pool=%p arg1=%lu arg2=%lu\n",
        ldr(), pRspQ, msg, msg->pPool, msg->arg1, msg->arg2);
    add(pRspQ, msg);
  } else {
    DPF(LDR "send_rsp_or_ret: no RspQ ret msg=%p pool=%p arg1=%lu arg2=%lu\n",
        ldr(), msg, msg->pPool, msg->arg1, msg->arg2);
    ret_msg(msg);
  }
}
