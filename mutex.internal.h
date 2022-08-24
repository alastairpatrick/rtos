#ifndef RTOS_MUTEX_INTERNAL_H
#define RTOS_MUTEX_INTERNAL_H

#include "base.h"
#include "scheduler.internal.h"

typedef struct Mutex {
  // Combines pointer to owning task and mutex state into a single 32-bit
  // variable to it can be transitioned atomically.
  atomic32_t owner_state;
  
  TaskSchedulingDList waiting;
} Mutex;

typedef struct ConditionVar {
  Mutex* mutex;
  TaskSchedulingDList waiting;
} ConditionVar;

#endif  // RTOS_MUTEX_INTERNAL_H