/**
 * This software is released into the public domain.
 */

#define NDEBUG

#define _DEFAULT_SOURCE
#define USE_RMV 1

#if USE_RMV
#define RMV rmv
#else
#define RMV rmv_non_stalling
#endif

#include "mpscfifo.h"
#include "msg_pool.h"
#include "dpf.h"

#include <sys/types.h>
#include <pthread.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

bool MsgPool_init(MsgPool_t* pool, uint32_t msg_count) {
  bool error;
  Msg_t* msgs;
  Cell_t* cells;

  DPF(LDR "MsgPool_init:+pool=%p msg_count=%u\n",
      ldr(), pool, msg_count);

  // Allocate messages
  msgs = malloc(sizeof(Msg_t) * msg_count);
  if (msgs == NULL) {
    printf(LDR "MsgPool_init:-pool=%p ERROR unable to allocate messages, aborting msg_count=%u\n",
        ldr(), pool, msg_count);
    error = true;
    goto done;
  }

  // Allocate cells
  cells = malloc(sizeof(Cell_t) * msg_count);
  if (cells == NULL) {
    printf(LDR "MsgPool_init:-pool=%p ERROR unable to allocate cells, aborting msg_count=%u\n",
        ldr(), pool, msg_count);
    error = true;
    goto done;
  }

  // Output info on the pool and messages
  DPF(LDR "MsgPool_init: pool=%p &msgs[0]=%p &msgs[1]=%p sizeof(Msg_t)=%lu(0x%lx)\n",
      ldr(), pool, &msgs[0], &msgs[1], sizeof(Msg_t), sizeof(Msg_t));

  // Create pool
  initMpscFifo(&pool->fifo);
  for (uint32_t i = 0; i < msg_count; i++) {
    Msg_t* msg = &msgs[i];
    msg->pCell = &cells[i];
    DPF(LDR "MsgPool_init: add %u msg=%p%s\n", ldr(), i, msg, i == 0 ? " stub" : "");
    //msg->pPoolFifo = &pool->fifo;
    msg->pPool = pool;
    add(&pool->fifo, msg);
  }

  pool->get_msg_count = 0;
  pool->ret_msg_count = 0;

  DPF(LDR "MsgPool_init: pool=%p, sizeof(*pool)=%lu(0x%lx)\n",
      ldr(), pool, sizeof(*pool), sizeof(*pool));

  error = false;
done:
  if (error) {
    free(msgs);
    pool->msgs = NULL;
    free(cells);
    pool->cells = NULL;
    pool->msg_count = 0;
  } else {
    pool->msgs = msgs;
    pool->cells = cells;
    pool->msg_count = msg_count;
  }

  DPF(LDR "MsgPool_init:-pool=%p msg_count=%u error=%u\n",
      ldr(), pool, msg_count, error);

  return error;
}

uint64_t MsgPool_deinit(MsgPool_t* pool) {
  DPF(LDR "MsgPool_deinit:+pool=%p msgs=%p fifo=%p\n", ldr(), pool, pool->msgs, &pool->fifo);
  uint64_t msgs_processed = 0;
#if 0
  msgs_processed += pool->fifo.msgs_processed;
  msgs_processed += pool->fifo.rb.msgs_processed;
#else
  if (pool->msgs != NULL) {
    // Empty the pool
    DPF(LDR "MsgPool_deinit: pool=%p pool->msg_count=%u get_msg_count=%u ret_msg_count=%u\n",
        ldr(), pool, pool->msg_count, pool->get_msg_count, pool->ret_msg_count);
    for (uint32_t i = 0; i < pool->msg_count; i++) {
      Msg_t* msg;

      // Wait until this is returned
      // TODO: Bug it may never be returned!
      bool once = false;
      while ((msg = RMV(&pool->fifo)) == NULL) {
        if (!once) {
          once = true;
          DPF(LDR "MsgPool_deinit: waiting for %u\n", ldr(), i);
        }
        sched_yield();
      }

      DPF(LDR "MsgPool_deinit: removed %u msg=%p\n", ldr(), i, msg);
    }

    DPF(LDR "MsgPool_deinit: pool=%p deinitMpscFifo pool->msg_count=%u get_msg_count=%u ret_msg_count=%u\n",
        ldr(), pool, pool->msg_count, pool->get_msg_count, pool->ret_msg_count);
    msgs_processed = deinitMpscFifo(&pool->fifo);

    // Free msgs
    DPF(LDR "MsgPool_deinit: pool=%p free msgs=%p\n", ldr(), pool, pool->msgs);
    free(pool->msgs);
    pool->msgs = NULL;

    // BUG: we can't free cells because the cells could be in use on other fifos.
    DPF(LDR "MsgPool_deinit: pool=%p CANNOT cells msgs=%p\n", ldr(), pool, pool->msgs);
    //free(pool->cells);
    //pool->cells = NULL;
    pool->msg_count = 0;
  }
  DPF(LDR "MsgPool_deinit:-pool=%p msgs_processed=%lu pool->msg_count=%u get_msg_count=%u ret_msg_count=%u\n",
        ldr(), pool, msgs_processed, pool->msg_count, pool->get_msg_count, pool->ret_msg_count);
#endif
  return msgs_processed;
}

Msg_t* MsgPool_get_msg(MsgPool_t* pool) {
  DPF(LDR "MsgPool_get_msg:+pool=%p\n", ldr(), pool);
  Msg_t* msg = RMV(&pool->fifo);
  if (msg != NULL) {
    msg->pRspQ = NULL;
    msg->arg1 = 0;
    msg->arg2 = 0;
    pool->get_msg_count += 1;
    DPF(LDR "MsgPool_get_msg: pool=%p got msg=%p pool=%p\n", ldr(), pool, msg, msg->pPool);
  }
  DPF(LDR "MsgPool_get_msg:-pool=%p msg=%p\n", ldr(), pool, msg);
  return msg;
}

void MsgPool_ret_msg(MsgPool_t* pool, Msg_t* pMsg) {
  DPF(LDR "MsgPool_ret_msg:+pool=%p msg=%p\n", ldr(), pool, pMsg);
  if (pMsg != NULL) {
    add(&pool->fifo, pMsg);
    pool->ret_msg_count += 1;
    DPF(LDR "MsgPool_ret_msg: pool=%p got msg=%p pool=%p\n", ldr(), pool, msg, msg->pPool);
  }
  DPF(LDR "MsgPool_ret_msg:-pool=%p msg=%p\n", ldr(), pool, pMsg);
}

