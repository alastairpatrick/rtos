#ifndef QOS_INTERRUPT_H
#define QOS_INTERRUPT_H

#include "base.h"

QOS_BEGIN_EXTERN_C

void qos_init_await_irq(int32_t irq);
bool qos_await_irq(int32_t irq, qos_atomic32_t* enable, int32_t mask, qos_time_t timeout);

QOS_END_EXTERN_C

#endif  // QOS_INTERRUPT_H
