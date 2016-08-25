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

#if 0 //defined(NDEBUG)
#define USE_COUNT 0
#else
#define USE_COUNT 1
#endif

#define DELAY 0

#include "crash.h"
#include "msg_pool.h"
#include "mpscfifo.h"
#include "mpscringbuff.h"
#include "mpsclinklist.h"
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
  rb_init(&pQ->rb, 0x2); //0x100);
  pQ->add_state = ADD_STATE_RB;
  pQ->rmv_state = RMV_STATE_RB;
  pQ->add_pending_count = 0;
  pQ->add_link_list_idx = 0;
  pQ->rmv_link_list_idx = 0;
  pQ->count = 0;
  return pQ;
}

/**
 * @see mpscfifo.h
 */
uint64_t deinitMpscFifo(MpscFifo_t* pQ) {
  uint32_t count = pQ->link_lists[0].count;
  count += pQ->link_lists[1].count;
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
  pQ->add_pending_count += 1;
  while (true) {
    uint32_t add_state = __atomic_load_n(&pQ->add_state, __ATOMIC_ACQUIRE);
    switch (add_state) {
      case (ADD_STATE_RB): {
        DPF(LDR "add: pQ=%p ADD_STATE_RB pMsg=%p\n", ldr(), pQ, pMsg);

        if (rb_add(&pQ->rb, pMsg)) {
#if USE_COUNT
          pQ->count += 1;
#endif
          pMsg->last_fifo_add_msg_pthread_id = pthread_self();
          pMsg->last_fifo_add_msg_fifo = pQ;
          pMsg->last_fifo_add_msg_state = ADD_STATE_RB;
          pMsg->last_fifo_add_msg_ll_idx = (uint64_t)-1;
          pMsg->last_fifo_add_msg_tick = gTick++;
          pQ->add_pending_count -= 1;
          DPF(LDR "add:-pQ=%p ADD_STATE_RB added pMsg=%p count=%d add_pending_count=%d\n",
              ldr(), pQ, pMsg, pQ->count, pQ->add_pending_count);
          return;
        }

        uint32_t add_state_rb = ADD_STATE_RB;
        if (__atomic_compare_exchange_n(&pQ->add_state, &add_state_rb, ADD_STATE_CHANGING_TO_LL, false, __ATOMIC_ACQ_REL, __ATOMIC_RELEASE)) {
          uint32_t idx = __atomic_load_n(&pQ->add_link_list_idx, __ATOMIC_ACQUIRE);
          idx ^= 1;
          __atomic_store_n(&pQ->add_link_list_idx, idx, __ATOMIC_RELEASE);
          __atomic_store_n(&pQ->add_state, ADD_STATE_LL, __ATOMIC_RELEASE);
          DPF(LDR "add: pQ=%p ADD_STATE_RB changed to ADD_STATE_LL pMsg=%p idx=%d\n", ldr(), pQ, pMsg, idx);
        } else {
          DPF(LDR "add: pQ=%p ADD_STATE_RB other producer changing pMsg=%p\n", ldr(), pQ, pMsg);
        }
        break;
      }

      case (ADD_STATE_CHANGING_TO_LL): {
        // Ring buffer is full, another producer is changing to the link list
        DPF(LDR "add: pQ=%p ADD_STATE_CHANGING_TO_LL pMsg=%p\n", ldr(), pQ, pMsg);
        break;
      }

      case (ADD_STATE_LL): {
        uint32_t idx = __atomic_load_n(&pQ->add_link_list_idx, __ATOMIC_ACQUIRE);
        ll_add(&pQ->link_lists[idx], pMsg);

#if USE_COUNT
        pQ->count += 1;
#endif
        pMsg->last_fifo_add_msg_pthread_id = pthread_self();
        pMsg->last_fifo_add_msg_fifo = pQ;
        pMsg->last_fifo_add_msg_state = ADD_STATE_LL;
        pMsg->last_fifo_add_msg_ll_idx = idx;
        pMsg->last_fifo_add_msg_tick = gTick++;
        pQ->add_pending_count -= 1;
        DPF(LDR "add:-pQ=%p ADD_STATE_LL pMsg=%p count=%d add_pending_count=%d\n",
            ldr(), pQ, pMsg, pQ->count, pQ->add_pending_count);
        return;
      }
    }
  }
}

/**
 * @see mpscifo.h
 */
Msg_t* rmv(MpscFifo_t* pQ) {
  Msg_t* pMsg;

  while (true) {
    switch (pQ->rmv_state) {
      case (RMV_STATE_RB): {
        DPF(LDR "rmv: pQ=%p RMV_STATE_RB\n", ldr(), pQ);
        pMsg = rb_rmv(&pQ->rb);
        if (pMsg != NULL) {
#if USE_COUNT
          pQ->count -= 1;
#endif
          DPF(LDR "rmv:-pQ=%p RMV_STATE_RB successful pMsg=%p count=%d\n", ldr(), pQ, pMsg, pQ->count);
          pMsg->last_fifo_rmv_msg_pthread_id = pthread_self();
          pMsg->last_fifo_rmv_msg_fifo = pQ;
          pMsg->last_fifo_rmv_msg_state = RMV_STATE_RB;
          pMsg->last_fifo_rmv_msg_tick = gTick++;
          return pMsg;
        }
        if (ADD_STATE_RB == __atomic_load_n(&pQ->add_state, __ATOMIC_ACQUIRE)) {
          // No messages in RB or LL
          DPF(LDR "rmv:-pQ=%p RMV_STATE_RB, empty pMsg=%p count=%d\n", ldr(), pQ, pMsg, pQ->count);
          return NULL;
        }

        // Rb is empty but the we need to switch to RMV_STATE_LL too
        pQ->rmv_link_list_idx = pQ->rmv_link_list_idx ^ 1;

        DPF(LDR "rmv: pQ=%p RMV_STATE_RB change to RMV_STATE_LL pMsg=%p\n", ldr(), pQ, pMsg);
        pQ->rmv_state = RMV_STATE_LL;

        break;
      }

      case (RMV_STATE_CHANGING_ADD_STATE_TO_ADD_STATE_RB): {
        DPF(LDR "rmv: pQ=%p RMV_STATE_CHANGING_ADD_STATE_TO_ADD_STATE_RB\n", ldr(), pQ);
        uint32_t add_state_ll = ADD_STATE_LL;
        if (__atomic_compare_exchange_n(&pQ->add_state, &add_state_ll, ADD_STATE_RB, false, __ATOMIC_ACQ_REL, __ATOMIC_RELEASE)) {
          DPF(LDR "rmv: pQ=%p add_state == ADD_STATE_RB\n", ldr(), pQ);
          pQ->rmv_state = RMV_STATE_CHANGING_TO_RB;
        } else {
          DPF(LDR "rmv: pQ=%p add_state != ADD_STATE_LL\n", ldr(), pQ);
          sched_yield();
        }
        break;
      }

      case (RMV_STATE_CHANGING_TO_RB): {
        DPF(LDR "rmv: pQ=%p RMV_STATE_CHANGING_TO_RB\n", ldr(), pQ);

        // Return any lingering messages from the link list
        pMsg = ll_rmv(&pQ->link_lists[pQ->rmv_link_list_idx]);
        uint32_t add_pending_count = 0;
        if (pMsg != NULL) {
#if USE_COUNT
          pQ->count -= 1;
#endif
          DPF(LDR "rmv:-pQ=%p RMV_STATE_CHANGING_TO_RB pMsg=%p count=%d\n", ldr(), pQ, pMsg, pQ->count);
          pMsg->last_fifo_rmv_msg_pthread_id = pthread_self();
          pMsg->last_fifo_rmv_msg_fifo = pQ;
          pMsg->last_fifo_rmv_msg_state = RMV_STATE_CHANGING_TO_RB;
          pMsg->last_fifo_rmv_msg_tick = gTick++;
          return pMsg;
        } else if (0 == (add_pending_count = pQ->add_pending_count)) {
          DPF(LDR "rmv: pQ=%p add_state == ADD_STATE_RB and LL is empty change to RMV_STATE_RB\n", ldr(), pQ);
          // link list is empty, now switch to RB
          pQ->rmv_state = RMV_STATE_RB;
        } else {
          DPF(LDR "rmv: pQ=%p add_pending_count=%d != 0\n", ldr(), pQ, add_pending_count);
          sched_yield();
        }

        break;
      }

      case (RMV_STATE_LL): {
        DPF(LDR "rmv: pQ=%p RMV_STATE_LL\n", ldr(), pQ);
        uint32_t idx = __atomic_load_n(&pQ->rmv_link_list_idx, __ATOMIC_ACQUIRE);
        pMsg = ll_rmv(&pQ->link_lists[idx]);
        if (pMsg != NULL) {
#if USE_COUNT
          pQ->count -= 1;
#endif
          DPF(LDR "rmv:-pQ=%p RMV_STATE_LL pMsg=%p count=%d\n", ldr(), pQ, pMsg, pQ->count);
          pMsg->last_fifo_rmv_msg_pthread_id = pthread_self();
          pMsg->last_fifo_rmv_msg_fifo = pQ;
          pMsg->last_fifo_rmv_msg_state = RMV_STATE_LL;
          pMsg->last_fifo_rmv_msg_tick = gTick++;
          return pMsg;
        }

        DPF(LDR "rmv: pQ=%p RMV_STATE_LL, change rmv_state=RMV_STATE_CHANGING_TO_RB\n", ldr(), pQ);
        pQ->rmv_state = RMV_STATE_CHANGING_ADD_STATE_TO_ADD_STATE_RB;

        break;
      }
    }
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
  msg->last_pRspQ = msg->pRspQ;
  msg->last_arg1 = msg->arg1;
  msg->last_arg2 = msg->arg2;

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
