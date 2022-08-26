#ifndef QOS_QUEUE_INTERNAL_H
#define QOS_QUEUE_INTERNAL_H

#include "semaphore.internal.h"
#include "mutex.internal.h"

QOS_BEGIN_EXTERN_C

typedef struct qos_queue_t {
  Semaphore read_semaphore;
  Semaphore write_semaphore;
  qos_mutex_t mutex;
  int32_t capacity;
  int32_t read_idx;
  int32_t write_idx;
  char *buffer;
} qos_queue_t;

QOS_END_EXTERN_C

#endif  // QOS_QUEUE_INTERNAL_H
