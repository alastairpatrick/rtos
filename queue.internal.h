#ifndef RTOS_QUEUE_INTERNAL_H
#define RTOS_QUEUE_INTERNAL_H

#include "semaphore.internal.h"
#include "mutex.internal.h"

BEGIN_EXTERN_C

typedef struct Queue {
  Semaphore read_semaphore;
  Semaphore write_semaphore;
  Mutex mutex;
  int32_t capacity;
  int32_t read_idx;
  int32_t write_idx;
  char *buffer;
} Queue;

END_EXTERN_C

#endif  // RTOS_QUEUE_INTERNAL_H