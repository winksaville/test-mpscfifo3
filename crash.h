/**
 * This software is released into the public domain.
 */

#ifndef _CRASH_H
#define _CRASH_H

#include <stdint.h>

#define CRASH() do { *((volatile uint8_t*)0) = 0; } while(0)

#endif
