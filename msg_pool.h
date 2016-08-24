/**
 * This software is released into the public domain.
 */

#ifndef _MSG_POOL_H
#define _MSG_POOL_H

#include "mpscfifo.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct MsgPool_t {
  Msg_t* msgs;
  Msg_t** msg_ptrs;
  Msg_t** owned_msgs;
  Cell_t* cells;
  uint32_t msg_count;
  volatile _Atomic(uint32_t) get_msg_count;
  volatile _Atomic(uint32_t) ret_msg_count;
  MpscFifo_t fifo;
} MsgPool_t;

bool MsgPool_init(MsgPool_t* pool, uint32_t msg_count);
uint64_t MsgPool_deinit(MsgPool_t* pool);
Msg_t* MsgPool_get_msg(MsgPool_t* pool);
void MsgPool_ret_msg(MsgPool_t* pool, Msg_t* pMsg);

#endif
