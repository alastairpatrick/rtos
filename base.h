#ifndef QOS_BASE_H
#define QOS_BASE_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "hardware/address_mapped.h"

#ifndef QOS_TICK_1MHZ_SOURCE
#define QOS_TICK_1MHZ_SOURCE 1      // nominal 1MHz clock source
#endif

#ifndef QOS_TICK_MS
#define QOS_TICK_MS 10
#endif

#ifndef QOS_TICK_CYCLES
#if QOS_TICK_1MHZ_SOURCE
#define QOS_TICK_CYCLES (QOS_TICK_MS * 1000)
#else
#define QOS_TICK_CYCLES (QOS_TICK_MS * 125 * 1000)
#endif
#endif

#ifndef QOS_MAX_EVENTS_PER_CORE
#define QOS_MAX_EVENTS_PER_CORE 8
#endif

#ifdef __cplusplus
#define QOS_BEGIN_EXTERN_C extern "C" {
#define QOS_END_EXTERN_C }
#else
#define QOS_BEGIN_EXTERN_C
#define QOS_END_EXTERN_C
#endif

#define STRIPED_RAM __attribute__((section(".time_critical.qos")))

typedef volatile int32_t qos_atomic32_t;
typedef void* volatile qos_atomic_ptr_t;

// <-1: Absolute time in us, starting at INT64_MIN
//  -1: Special value meaning no timeout
//   0: Special value usually meaning return immediately rather than block
//   1: Special value meaning timeout next tick
//  >0: Duration in us
#define QOS_NO_TIMEOUT -1LL
#define QOS_NO_BLOCKING 0LL
#define QOS_TIMEOUT_NEXT_TICK 1LL
typedef int64_t qos_time_t;

typedef enum qos_error_t {
  QOS_SUCCESS,
  QOS_TIMEOUT,
} qos_error_t;

typedef enum qos_task_state_t {
  QOS_TASK_RUNNING,
  QOS_TASK_READY,
  QOS_TASK_BUSY_BLOCKED,
  QOS_TASK_SYNC_BLOCKED,
} qos_task_state_t;

typedef void (*qos_proc_t)();
typedef void (*qos_proc_int32_t)(int32_t);

#endif  // QOS_BASE_H
