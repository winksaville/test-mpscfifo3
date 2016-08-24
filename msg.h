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
  uint8_t data[];
} Msg_t;

#endif
