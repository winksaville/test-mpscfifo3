/**
 * This software is released into the public domain.
 */

#ifndef _MSG_H
#define _MSG_H

#include <stdint.h>

// Forward declarations
typedef struct Cell_t Cell_t;
typedef struct MpscFifo_t MpscFifo_t;
typedef struct Msg_t Msg_t;
typedef struct MsgPool_t MsgPool_t;

typedef struct Cell_t {
  union {
    Cell_t* pNext __attribute__ (( aligned (64) ));
    uint32_t seq;
  };
  Msg_t* pMsg;
} Cell_t;

typedef struct Msg_t {
  Cell_t* pCell;
  //MpscFifo_t* pPoolFifo;
  MsgPool_t* pPool;
  MpscFifo_t* pRspQ;
  uint64_t arg1;
  uint64_t arg2;

  MpscFifo_t* last_pRspQ;
  uint64_t last_arg1;
  uint64_t last_arg2;
  uint64_t last_MsgPool_get_msg_pthread_id;
  uint64_t last_MsgPool_get_msg_tick;
  uint64_t last_MsgPool_ret_msg_pthread_id;
  uint64_t last_MsgPool_ret_msg_tick;

  uint64_t last_fifo_add_msg_pthread_id;
  MpscFifo_t* last_fifo_add_msg_fifo;
  uint64_t last_fifo_add_msg_state;
  uint64_t last_fifo_add_msg_ll_idx;
  uint64_t last_fifo_add_msg_tick;
  uint64_t last_fifo_rmv_msg_pthread_id;
  MpscFifo_t* last_fifo_rmv_msg_fifo;
  uint64_t last_fifo_rmv_msg_state;
  uint64_t last_fifo_rmv_msg_tick;

  uint8_t data[];
} Msg_t;

#endif
