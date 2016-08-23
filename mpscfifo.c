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
#include "mpsclinklist.h"
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
MpscFifo_t* initMpscFifo(MpscFifo_t* pQ) {
  printf(LDR "initMpscFifo:*pQ=%p\n", ldr(), pQ);
  ll_init(&pQ->link_lists[0]);
  ll_init(&pQ->link_lists[1]);
  rb_init(&pQ->rb, 0x100);
  pQ->add_use_rb = true;
  pQ->rmv_use_rb = true;
  return pQ;
}

/**
 * @see mpscfifo.h
 */
uint64_t deinitMpscFifo(MpscFifo_t* pQ) {
  uint32_t count = pQ->link_lists[0].count;
  count += pQ->link_lists[0].count;
  count += pQ->rb.count;

  uint64_t msgs_processed = ll_deinit(&pQ->link_lists[0]);
  msgs_processed += ll_deinit(&pQ->link_lists[1]);
  msgs_processed += rb_deinit(&pQ->rb);

  printf(LDR "deinitMpscFifo:-pQ=%p count=%u msgs_processed=%lu\n", ldr(), pQ, count, msgs_processed);
  return msgs_processed;
}

/**
 * @see mpscifo.h
 */
void add(MpscFifo_t* pQ, Msg_t* pMsg) {
  if (__atomic_load_n((bool*)&pQ->add_use_rb, __ATOMIC_ACQUIRE)) {
    if (rb_add(&pQ->rb, pMsg)) {
      DPF(LDR "add: pQ=%p added pMsg=%p using rb_add\n", ldr(), pQ, pMsg);
    } else {
      // Ring buffer is full, have one producer switch to one of the link lists
      bool changing = false;
      if (__atomic_compare_exchange_n(&pQ->add_link_list_changing, &changing, true, false, __ATOMIC_ACQ_REL, __ATOMIC_RELEASE)) {
        DPF(LDR "add: pQ=%p changing, DOING pMsg=%p\n", ldr(), pQ, pMsg);
        pQ->add_link_list_idx = !pQ->add_link_list_idx;
        __atomic_store_n((bool*)&pQ->add_use_rb, false, __ATOMIC_RELEASE);
        __atomic_store_n((bool*)&pQ->add_link_list_changing, true, __ATOMIC_RELEASE);
      } else {
        DPF(LDR "add: pQ=%p changing, WAITING pMsg=%p\n", ldr(), pQ, pMsg);
        while(true == __atomic_load_n(&pQ->add_link_list_changing, __ATOMIC_ACQUIRE)) {
          // TODO: FIX we must not spin but good enough for now.
        }
      }

      ll_add(&pQ->link_lists[pQ->add_link_list_idx], pMsg);
    }
  }
}

/**
 * @see mpscifo.h
 */
Msg_t* rmv(MpscFifo_t* pQ) {
  Msg_t* pMsg;
  if (__atomic_load_n((bool*)&pQ->rmv_use_rb, __ATOMIC_ACQUIRE)) {
    pMsg = rb_rmv(&pQ->rb);
    if (pMsg != NULL) {
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
  } else {
    pMsg = ll_rmv(&pQ->link_lists[pQ->add_link_list_idx]);
  }
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
      CRASH();
    } else {
      printf(LDR "ret:#No pool msg=%p pool=%p arg1=%lu arg2=%lu\n",
          ldr(), pMsg, pMsg->pPool, pMsg->arg1, pMsg->arg2);
      CRASH();
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
