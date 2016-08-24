/**
 * This software is released into the public domain.
 *
 * A MpscRingBuff is a wait free/thread safe multi-producer
 * single consumer ring buffer.
 *
 * The ring buffer has a head and tail, the elements are added
 * to the head removed from the tail.
 */

#ifndef COM_SAVILLE_MPSCRINGBUFF_H
#define COM_SAVILLE_MPSCRINGBUFF_H

#include "msg.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct MpscRingBuff_t MpscRingBuff_t;
typedef struct Msg_t Msg_t;

typedef struct MpscRingBuff_t {
  uint32_t volatile add_idx __attribute__(( aligned (64) ));
  uint32_t volatile rmv_idx __attribute__(( aligned (64) ));
  uint32_t size;
  uint32_t mask;
  Cell_t* ring_buffer;
  volatile _Atomic(uint32_t) count;
  volatile _Atomic(uint64_t) msgs_processed;
} MpscRingBuff_t;

/**
 * Initialize the MpscRingBuff_t, size must be a power of two.
 *
 * @return NULL if an error if size cannot be malloced or is not a power of 2.
 */
extern MpscRingBuff_t* rb_init(MpscRingBuff_t* pRb, uint32_t size);

/**
 * Deinitialize the MpscRingBuff_t, assumes the ring buffer is empty.
 *
 * @return number of messages removed.
 */
extern uint64_t rb_deinit(MpscRingBuff_t *pQ);

/**
 * Add a Msg_t to the ring buffer
 *
 * @return true if added return false if full
 */
extern bool rb_add(MpscRingBuff_t* pRb, Msg_t* pMsg);

/**
 * Remove a Msg_t from the ring buffer. This maybe used only by
 * a single thread.
 *
 * @return NULL if empty.
 */
extern Msg_t *rb_rmv(MpscRingBuff_t *pQ);

#endif
