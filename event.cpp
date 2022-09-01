#include "event.h"
#include "event.internal.h"

#include "atomic.h"
#include "core_migrator.h"
#include "svc.h"
#include "time.h"

#include "hardware/structs/scb.h"

static volatile bool g_signalled[NUM_CORES][QOS_MAX_EVENTS_PER_CORE];
static qos_event_t* g_events[NUM_CORES][QOS_MAX_EVENTS_PER_CORE];
static int8_t g_next_idx[NUM_CORES];

extern "C" {
  void qos_internal_atomic_write_fifo(qos_task_t*);
}

qos_event_t* qos_new_event(int32_t core) {
  auto event = new qos_event_t;
  qos_init_event(event, core);
  return event;
}

void qos_init_event(qos_event_t* event, int32_t core) {
  if (core < 0) {
    core = get_core_num();
  }
  event->core = core;

  qos_init_dlist(&event->waiting.tasks);

  assert(g_next_idx[core] < QOS_MAX_EVENTS_PER_CORE);
  auto idx = g_next_idx[core]++;
  event->signalled = &g_signalled[core][idx];
  g_events[core][idx] = event;
}

static qos_task_state_t STRIPED_RAM await_event_supervisor(qos_scheduler_t* scheduler, va_list args) {
  auto event = va_arg(args, qos_event_t*);
  auto timeout = va_arg(args, qos_time_t);

  assert(timeout != 0);
  assert(qos_is_dlist_empty(&event->waiting.tasks));

  auto current_task = scheduler->current_task;

  if (*event->signalled) {
    *event->signalled = false;
    qos_current_supervisor_call_result(scheduler, true);
    return QOS_TASK_RUNNING;
  }

  qos_internal_insert_scheduled_task(&event->waiting, current_task);
  qos_delay_task(scheduler, current_task, timeout);

  return QOS_TASK_SYNC_BLOCKED;
}

bool qos_await_event(qos_event_t* event, qos_time_t timeout) {
  qos_normalize_time(&timeout);

  qos_core_migrator migrator(event->core);

  if (*event->signalled) {
    *event->signalled = false;
    return true;
  }

  if (timeout == 0) {
    return false;
  }

  return qos_call_supervisor_va(await_event_supervisor, event, timeout);
}

bool qos_internal_check_signalled_events_supervisor(qos_scheduler_t* scheduler) {
  auto core = get_core_num();

  auto should_yield = false;
  for (auto i = 0; i < QOS_MAX_EVENTS_PER_CORE; ++i) {
    if (g_signalled[core][i]) {
      auto event = g_events[core][i];
      auto waiting = begin(event->waiting);
      if (!empty(waiting)) {
        should_yield |= qos_ready_task(scheduler, &*waiting);
      }
    }
  }

  return should_yield;
}

qos_task_state_t signal_event_supervisor(qos_scheduler_t* scheduler, void* p) {
  auto event = (qos_event_t*) p;

  auto waiting = begin(event->waiting);
  if (empty(waiting)) {
    *event->signalled = true;
    return QOS_TASK_RUNNING;
  } else {
    bool should_yield = qos_ready_task(scheduler, &*waiting);
    if (should_yield) {
      return QOS_TASK_READY;
    } else {
      return QOS_TASK_RUNNING;
    }
  }
}

void qos_signal_event(qos_event_t* event) {
  if (event->core == get_core_num()) {
    qos_call_supervisor(signal_event_supervisor, event);
  } else {
    *event->signalled = true;
    qos_internal_atomic_write_fifo(0);
  }
}