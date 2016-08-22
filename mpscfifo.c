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
#include "msg_pool.h"
#include "dpf.h"

#include <sys/types.h>
#include <pthread.h>

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
  printf(LDR "initMpscFifo:*pQ=%p\n", ldr(), pQ);
  pQ->cell.pNext = NULL;
  pQ->cell.pMsg = NULL;
  pQ->pHead = &pQ->cell;
  pQ->pTail = &pQ->cell;
  pQ->count = 0;
  pQ->msgs_processed = 0;
  pQ->add_use_rb = true;
  pQ->rmv_use_rb = true;
  rb_init(&pQ->rb, 0x100);
  return pQ;
}

/**
 * @see mpscfifo.h
 */
uint64_t deinitMpscFifo(MpscFifo_t *pQ) {
  uint64_t msgs_processed = pQ->msgs_processed;
  msgs_processed += rb_deinit(&pQ->rb);
  pQ->add_use_rb = false;
  pQ->rmv_use_rb = false;
//#ifndef NDEBUG
  uint32_t count = pQ->count;
//#endif
  Cell_t *pStub = pQ->pHead;
  pStub->pNext = NULL;
  pQ->pHead = NULL;
  pQ->pTail = NULL;
  pQ->count = 0;
  pQ->msgs_processed = 0;

  printf(LDR "deinitMpscFifo:-pQ=%p count=%u msgs_processed=%lu\n", ldr(), pQ, count, msgs_processed);
  return msgs_processed;
}

/**
 * @see mpscifo.h
 */
void add(MpscFifo_t *pQ, Msg_t *pMsg) {
#if 0
    if (rb_add(&pQ->rb, pMsg)) {
      pQ->count += 1;
      return;
    } else {
      return;
    }
#else
  if (__atomic_load_n((bool*)&pQ->add_use_rb, __ATOMIC_ACQUIRE)) {
    if (rb_add(&pQ->rb, pMsg)) {
      DPF(LDR "add: pQ=%p added pMsg=%p using rb_add\n", ldr(), pQ, pMsg);
      pQ->count += 1;
      return;
    }
    printf(LDR "add: pQ=%p set use_rb=false FULL add to fifo pMsg=%p\n", ldr(), pQ, pMsg);
    __atomic_store_n((bool*)&pQ->add_use_rb, false, __ATOMIC_RELEASE);
  }
  Cell_t* pCell = pMsg->pCell;
  pCell->pNext = NULL;
  pCell->pMsg = pMsg;
  Cell_t* pPrev = __atomic_exchange_n(&pQ->pHead, pCell, __ATOMIC_ACQ_REL);
  // rmv will stall spinning if preempted at this critical spot
  __atomic_store_n(&pPrev->pNext, pCell, __ATOMIC_RELEASE);
  pQ->count += 1;
#endif
}

#if 0
/**
 * @see mpscifo.h
 */
Msg_t *rmv_non_stalling(MpscFifo_t *pQ) {
  Msg_t* pMsg;

  if (__atomic_load_n(&pQ->use_rb, __ATOMIC_ACQUIRE)) {
    pMsg = rb_rmv(&pQ->rb);
    if (pMsg != NULL) {
      pQ->count -= 1;
    }
    return pMsg;
  }
  Cell_t* pTail = pQ->pTail;
  Cell_t* pNext = pTail->pNext;
  if (pNext != NULL) {
    pMsg = pNext->pMsg;
    pMsg->pCell = pTail;
    pQ->pTail = pNext;
  } else {
    pMsg = rb_rmv(&pQ->rb);
    if (pMsg == NULL) {
      CRASH();
    }
    pQ->use_rb = true;
  }
  if (pMsg != NULL) {
    pQ->count -= 1;
  }
  pQ->msgs_processed += 1;
  return pMsg;
}
#endif

/**
 * @see mpscifo.h
 */
Msg_t *rmv(MpscFifo_t *pQ) {
#if 0
  return rb_rmv(&pQ->rb);
#else
  Msg_t* pMsg;
  if (__atomic_load_n((bool*)&pQ->rmv_use_rb, __ATOMIC_ACQUIRE)) {
    pMsg = rb_rmv(&pQ->rb);
    if (pMsg != NULL) {
      pQ->count -= 1;
      DPF(LDR "rmv: pQ=%p 0.0 pMsg=%p\n", ldr(), pQ, pMsg);
      return pMsg;
    }
    if (__atomic_load_n((bool*)&pQ->add_use_rb, __ATOMIC_ACQUIRE)) {
      // Empty, but since its never been full we keep using rb
      return NULL;
    }
    // Empty, and its was full so stop using rb
    printf(LDR "rmv: pQ=%p 1.0 pMsg=%p\n", ldr(), pQ, pMsg);
    __atomic_store_n((bool*)&pQ->rmv_use_rb, false, __ATOMIC_RELEASE);
  }
  Cell_t* pTail = pQ->pTail;
  Cell_t* pNext = pTail->pNext;
  if ((pNext == NULL) && (pTail == __atomic_load_n(&pQ->pHead, __ATOMIC_ACQUIRE))) {
    return NULL;
#if 0
    pMsg = rb_rmv(&pQ->rb);
    //printf(LDR "add: set use_rb=true\n", ldr());
    if (pMsg == NULL) {
      //printf(LDR "rmv: 2.0 pMsg=%p\n", ldr(), pMsg);
      //CRASH(); // 1
      //printf(LDR "rmv: 2.1\n", ldr());
      //__atomic_store_n(&pQ->use_rb, true, __ATOMIC_RELEASE);
      //__atomic_store_n(&pQ->use_rb, true, __ATOMIC_RELEASE);
      return pMsg;
    } else {
      pQ->count -= 1;
      pQ->msgs_processed += 1;
      return pMsg;
    }
#endif
  } else {
    if (pNext == NULL) {
      while ((pNext = __atomic_load_n(&pTail->pNext, __ATOMIC_ACQUIRE)) == NULL) {
        sched_yield();
      }
    }
    pMsg = pNext->pMsg;
    pMsg->pCell = pTail;
    pQ->pTail = pNext;
    if (pMsg == NULL) {
      printf(LDR "rmv: pQ=%p 3.0\n", ldr(), pQ);
      CRASH(); // 2
      printf(LDR "rmv: pQ=%p 3.1\n", ldr(), pQ);
    }
    pQ->count -= 1;
    pQ->msgs_processed += 1;
    return pMsg;
  }
#endif
}

/**
 * @see mpscfifo.h
 */
void ret_msg(Msg_t* pMsg) {
  if ((pMsg != NULL) && (pMsg->pPool != NULL)) {
    DPF(LDR "ret_msg: pool=%p msg=%p arg1=%lu arg2=%lu\n", ldr(), pMsg->pPool, pMsg, pMsg->arg1, pMsg->arg2);
    MsgPool_ret_msg(pMsg->pPool, pMsg);
    //add(pMsg->pPoolFifo, pMsg);
  } else {
    if (pMsg == NULL) {
      printf(LDR "ret:#No msg msg=%p\n", ldr(), pMsg);
    } else {
      printf(LDR "ret:#No pool msg=%p pool=%p arg1=%lu arg2=%lu\n",
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
