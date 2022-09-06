#ifndef QOS_CONFIG_H
#define QOS_CONFIG_H

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

#ifndef QOS_EXCEPTION_STACK_SIZE
#define QOS_EXCEPTION_STACK_SIZE (PICO_STACK_SIZE - 256)
#endif

#ifndef QOS_SYSTICK_CHECKS_STACK_OVERFLOW
#ifdef NDEBUG
#define QOS_SYSTICK_CHECKS_STACK_OVERFLOW 0
#else
#define QOS_SYSTICK_CHECKS_STACK_OVERFLOW 1
#endif
#endif

#ifndef QOS_APP_MPU_REGIONS
#define QOS_APP_MPU_REGIONS 0
#endif

#ifndef QOS_PROTECT_TASK_STACK
#define QOS_PROTECT_TASK_STACK 1
#endif

#ifndef QOS_PROTECT_EXCEPTION_STACK
#define QOS_PROTECT_EXCEPTION_STACK 1
#endif

#ifndef QOS_PROTECT_IDLE_STACK
#define QOS_PROTECT_IDLE_STACK 1
#endif

#ifndef QOS_PROTECT_CORE0_SCRATCH_BANK
#define QOS_PROTECT_CORE0_SCRATCH_BANK 1
#endif

#ifndef QOS_PROTECT_CORE1_SCRATCH_BANK
#define QOS_PROTECT_CORE1_SCRATCH_BANK 1
#endif

#endif  // QOS_CONFIG_H
