/**
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

#ifndef COM_SAVILLE_MPSCFIFO_H
#define COM_SAVILLE_MPSCFIFO_H

#include "msg.h"
#include "mpscringbuff.h"
#include "mpsclinklist.h"

#include <stdbool.h>
#include <stdint.h>

#define ADD_STATE_RB               1
#define ADD_STATE_LL               2
#define ADD_STATE_CHANGING_TO_LL   3

#define RMV_STATE_RB               4
#define RMV_STATE_LL               5
#define RMV_STATE_CHANGING_TO_RB   6

typedef struct MpscFifo_t {
  MpscRingBuff_t rb;
  uint32_t add_state;
  uint32_t rmv_state;

  MpscLinkList_t link_lists[2];

  uint32_t add_link_list_idx;
  uint32_t rmv_link_list_idx;

  volatile _Atomic(int32_t) count;
} MpscFifo_t;
  
/**
 * Initialize an MpscFifo_t. Don't forget to empty the fifo
 * and delete the stub before freeing MpscFifo_t.
 */
extern MpscFifo_t* initMpscFifo(MpscFifo_t* pQ);

/**
 * Deinitialize the MpscFifo_t and ***pStub is stub if this routine
 * can't return it to its pool (ppStub maybe NULL).  Assumes the
 * fifo is empty and the only member is the stub.
 *
 * @return number of messages removed.
 */
extern uint64_t deinitMpscFifo(MpscFifo_t* pQ);

/**
 * Add a Msg_t to the Queue. This maybe used by multiple
 * entities on the same or different thread. This will never
 * block as it is a wait free algorithm.
 */
extern void add(MpscFifo_t* pQ, Msg_t* pMsg);

/**
 * Remove a Msg_t from the Queue. This maybe used only by
 * a single thread and returns NULL if empty or would
 * have blocked.
 */
extern Msg_t* rmv_non_stalling(MpscFifo_t* pQ);

/**
 * Remove a Msg_t from the Queue. This maybe used only by
 * a single thread and returns NULL if empty. This may
 * stall if a producer call add and was preempted before
 * finishing.
 */
extern Msg_t* rmv(MpscFifo_t* pQ);

/**
 * Return the message to its pool.
 */
extern void ret_msg(Msg_t* pMsg);

/**
 * Send a response arg1 if the msg->pRspQ != NULL otherwise ret msg
 */
extern void send_rsp_or_ret(Msg_t* msg, uint64_t arg1);

#endif
