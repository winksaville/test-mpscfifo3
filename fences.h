/**
 * This software is released into the public domain.
 */

#ifndef _FENCES_H
#define _FENCES_H

/** mfence instruction */
static inline void mfence(void) {
  __asm__ volatile ("mfence": : :"memory");
}

/** lfence instruction */
static inline void lfence(void) {
  __asm__ volatile ("lfence": : :"memory");
}

/** sfence instruction */
static inline void sfence(void) {
  __asm__ volatile ("sfence": : :"memory");
}

#endif
