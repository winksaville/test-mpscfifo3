/**
 * This software is released into the public domain.
 *
 * A MpscLinkList is a wait free/thread safe multi-producer
 * single consumer first in first out queue using a link list.
 * This algorithm is from Dimitry Vyukov's non intrusive MPSC code here:
 *   http://www.1024cores.net/home/lock-free-algorithms/queues/non-intrusive-mpsc-node-based-queue
 */

#ifndef COM_SAVILLE_MPSC_LINK_LIST_H
#define COM_SAVILLE_MPSC_LINK_LIST_H

#include "msg.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct MpscLinkList_t {
  Cell_t* pHead __attribute__(( aligned (64) ));
  Cell_t* pTail __attribute__(( aligned (64) ));
  volatile _Atomic(uint32_t) count;
  volatile _Atomic(uint64_t) msgs_processed;
  Cell_t cell;
} MpscLinkList_t;


/**
 * Initialize an MpscLinkList_t. Don't forget to empty the fifo
 * and delete the stub before freeing MpscLinkList_t.
 */
extern MpscLinkList_t* ll_init(MpscLinkList_t* pLl);

/**
 * Deinitialize the MpscLinkList_t.
 *
 * @return number of messages removed.
 */
extern uint64_t ll_deinit(MpscLinkList_t* pLl);

/**
 * Add a Msg_t to the head of the link list. This maybe used by multiple
 * entities on the same or different thread. This will never
 * block as it is a wait free algorithm.
 */
extern void ll_add(MpscLinkList_t* pLl, Msg_t* pMsg);

/**
 * Remove a Msg_t from the tail of the link list. This maybe used only by
 * a single thread and returns NULL if empty. This may
 * stall if a producer call add and was preempted before
 * finishing.
 */
extern Msg_t* ll_rmv(MpscLinkList_t* pLl);

#endif
