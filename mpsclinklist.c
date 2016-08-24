/*
 * This software is released into the public domain.
 *
 * A MpscLinkList_t is a wait free/thread safe multi-producer
 * single consumer first in first out queue using a link list.
 * This algorithm is from Dimitry Vyukov's non intrusive MPSC code here:
 *   http://www.1024cores.net/home/lock-free-algorithms/queues/non-intrusive-mpsc-node-based-queue
 */

#define NDEBUG

#define _DEFAULT_SOURCE

#define DELAY 0

#include "mpsclinklist.h"
#include "crash.h"
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
 * @see mpsclinklist.h
 */
MpscLinkList_t* ll_init(MpscLinkList_t* pLl) {
  printf(LDR "ll_init:+pLl=%p\n", ldr(), pLl);

  pLl->cell.pNext = NULL;
  pLl->cell.pMsg = NULL;
  pLl->pHead = &pLl->cell;
  pLl->pTail = &pLl->cell;
  pLl->count = 0;
  pLl->msgs_processed = 0;

  printf(LDR "ll_init:-pLl=%p\n", ldr(), pLl);
  return pLl;
}

/**
 * @see mpsclinklist.h
 */
uint64_t ll_deinit(MpscLinkList_t* pLl) {
  printf(LDR "ll_deinit:+pLl=%p\n", ldr(), pLl);

  uint64_t msgs_processed = pLl->msgs_processed;
  uint32_t count = pLl->count;

  Cell_t* pStub = pLl->pHead;
  pStub->pNext = NULL;
  pLl->pHead = NULL;
  pLl->pTail = NULL;
  pLl->count = 0;
  pLl->msgs_processed = 0;

  printf(LDR "ll_deinit:-pLl=%p count=%u msgs_processed=%lu\n", ldr(), pLl, count, msgs_processed);
  return msgs_processed;
}

/**
 * @see mpsclinklist.h
 */
void ll_add(MpscLinkList_t* pLl, Msg_t* pMsg) {
  DPF(LDR "ll_add:+pLl=%p pMsg=%p\n", ldr(), pLl, pMsg);

  Cell_t* pCell = pMsg->pCell;
  pCell->pNext = NULL;
  pCell->pMsg = pMsg;
  Cell_t* pPrev = __atomic_exchange_n(&pLl->pHead, pCell, __ATOMIC_ACQ_REL);
  // rmv will stall spinning if preempted at this critical spot
  __atomic_store_n(&pPrev->pNext, pCell, __ATOMIC_RELEASE);
  pLl->count += 1;

  DPF(LDR "ll_add:-pLl=%p pMsg=%p\n", ldr(), pLl, pMsg);
}

/**
 * @see mpsclinklist.h
 */
Msg_t* ll_rmv(MpscLinkList_t* pLl) {
  DPF(LDR "ll_rmv:+pLl=%p\n", ldr(), pLl);

  Msg_t* pMsg;
  Cell_t* pTail = pLl->pTail;
  Cell_t* pNext = pTail->pNext;
  if ((pNext == NULL) && (pTail == __atomic_load_n(&pLl->pHead, __ATOMIC_ACQUIRE))) {
    DPF(LDR "ll_rmv:-pLl=%p EMPTY\n", ldr(), pLl);
    return NULL;
  } else {
    if (pNext == NULL) {
      while ((pNext = __atomic_load_n(&pTail->pNext, __ATOMIC_ACQUIRE)) == NULL) {
        sched_yield();
      }
    }
    pMsg = pNext->pMsg;
    pMsg->pCell = pTail;
    pLl->pTail = pNext;
    if (pMsg == NULL) {
      printf(LDR "ll_rmv: pLl=%p WTF 1 pMsg == NULL\n", ldr(), pLl);
      CRASH();
      printf(LDR "ll_rmv: pLl=%pWTF 2 pMsg == NULL\n", ldr(), pLl);
    }
    pLl->count -= 1;
    pLl->msgs_processed += 1;
    DPF(LDR "ll_rmv:-pLl=%p pMsg=%p\n", ldr(), pLl, pMsg);
    return pMsg;
  }
}
