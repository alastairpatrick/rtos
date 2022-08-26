#include "mutex.h"
#include "mutex.internal.h"

#include "atomic.h"
#include "critical.h"
#include "dlist_it.h"
#include "scheduler.h"
#include "scheduler.internal.h"

//////// Mutex ////////

enum mutex_state_t {
  ACQUIRED_UNCONTENDED,
  AVAILABLE,
  ACQUIRED_CONTENDED,
};

Mutex* qos_new_mutex() {
  auto mutex = new Mutex;
  qos_init_mutex(mutex);
  return mutex;
}

void qos_init_mutex(Mutex* mutex) {
  mutex->core = get_core_num();
  mutex->owner_state = AVAILABLE;
  qos_init_dlist(&mutex->waiting.tasks);
}

static Task* STRIPED_RAM unpack_owner(int32_t owner_state) {
  return (Task*) (owner_state & ~3);
}

static mutex_state_t STRIPED_RAM unpack_state(int32_t owner_state) {
  return mutex_state_t(owner_state & 3);
}

static int32_t STRIPED_RAM pack_owner_state(Task* owner, mutex_state_t state) {
  return int32_t(owner) | state;
}

static qos_task_state_t STRIPED_RAM acquire_mutex_critical(Scheduler* scheduler, va_list args) {
  auto mutex = va_arg(args, Mutex*);
  auto timeout = va_arg(args, qos_tick_count_t);
  
  auto current_task = scheduler->current_task;

  auto owner_state = mutex->owner_state;
  auto owner = unpack_owner(owner_state);
  auto state = unpack_state(owner_state);

  if (state == AVAILABLE) {
    mutex->owner_state = pack_owner_state(current_task, ACQUIRED_UNCONTENDED);
    qos_set_current_critical_section_result(scheduler, true);
    return TASK_RUNNING;
  }

  if (timeout == 0) {
    return TASK_RUNNING;
  }

  mutex->owner_state = pack_owner_state(owner, ACQUIRED_CONTENDED);

  internal_insert_scheduled_task(&mutex->waiting, current_task);
  delay_task(scheduler, current_task, timeout);

  return TASK_SYNC_BLOCKED;
}

bool STRIPED_RAM qos_acquire_mutex(Mutex* mutex, qos_tick_count_t timeout) {
  assert(mutex->core == get_core_num());
  assert(!owns_mutex(mutex));
  check_tick_count(&timeout);

  auto current_task = get_current_task();

  if (qos_atomic_compare_and_set(&mutex->owner_state, AVAILABLE, pack_owner_state(current_task, ACQUIRED_UNCONTENDED)) == AVAILABLE) {
    return true;
  }
  
  if (timeout == 0) {
    return false;
  }

  return qos_critical_section_va(acquire_mutex_critical, mutex, timeout);
}

static qos_task_state_t STRIPED_RAM release_mutex_critical(Scheduler* scheduler, void* m) {
  auto mutex = (Mutex*) m;

  auto current_task = scheduler->current_task;

  auto owner_state = mutex->owner_state;
  auto owner = unpack_owner(owner_state);
  auto state = unpack_state(owner_state);

  if (state == ACQUIRED_UNCONTENDED) {
    mutex->owner_state = pack_owner_state(nullptr, AVAILABLE);
    return TASK_RUNNING;
  }

  assert(state == ACQUIRED_CONTENDED);

  auto resumed = &*begin(mutex->waiting);
  qos_set_critical_section_result(scheduler, resumed, true);
  bool should_yield = ready_task(scheduler, resumed);

  state = empty(begin(mutex->waiting)) ? ACQUIRED_UNCONTENDED : ACQUIRED_CONTENDED;
  mutex->owner_state = pack_owner_state(resumed, state);

  if (should_yield) {
    return TASK_READY;
  } else {
    return TASK_RUNNING;
  }
}

void STRIPED_RAM qos_release_mutex(Mutex* mutex) {
  assert(owns_mutex(mutex));

  auto current_task = get_current_task();

  // Fast path when no tasks waiting.
  int32_t expected = pack_owner_state(current_task, ACQUIRED_UNCONTENDED);
  if (qos_atomic_compare_and_set(&mutex->owner_state, expected, AVAILABLE) == expected) {
    return;
  }

  qos_critical_section(release_mutex_critical, mutex);
}

bool STRIPED_RAM owns_mutex(Mutex* mutex) {
  return unpack_owner(mutex->owner_state) == get_current_task();
}


//////// ConditionVar ////////

ConditionVar* qos_new_condition_var(Mutex* mutex) {
  auto var = new ConditionVar;
  qos_init_condition_var(var, mutex);
  return var;
}

void qos_init_condition_var(ConditionVar* var, Mutex* mutex) {
  var->mutex = mutex;
  qos_init_dlist(&var->waiting.tasks);
}

void qos_acquire_condition_var(struct ConditionVar* var, qos_tick_count_t timeout) {
  qos_acquire_mutex(var->mutex, timeout);
}

qos_task_state_t qos_wait_condition_var_critical(Scheduler* scheduler, va_list args) {
  auto var = va_arg(args, ConditionVar*);
  auto timeout = va_arg(args, qos_tick_count_t);

  auto current_task = scheduler->current_task;

  // _Atomically_ release mutex and add to condition variable waiting list.

  release_mutex_critical(scheduler, var->mutex);

  internal_insert_scheduled_task(&var->waiting, current_task);
  delay_task(scheduler, current_task, timeout);

  return TASK_SYNC_BLOCKED;
}

bool qos_wait_condition_var(ConditionVar* var, qos_tick_count_t timeout) {
  assert(owns_mutex(var->mutex));
  assert(timeout != 0);
  check_tick_count(&timeout);

  return qos_critical_section_va(qos_wait_condition_var_critical, var, timeout);
}

void qos_release_condition_var(ConditionVar* var) {
  qos_release_mutex(var->mutex);
}

static qos_task_state_t release_and_signal_condition_var_critical(Scheduler* scheduler, void* v) {
  auto var = (ConditionVar*) v;

  auto current_task = scheduler->current_task;

  auto signalled_it = begin(var->waiting);
  if (!empty(signalled_it)) {

    // The current task owns the mutex so the signalled task is not immediately
    // ready. Rather it is moved from the condition variable's waiting list to
    // the mutex's.
    auto signalled_task = &*signalled_it;
    internal_insert_scheduled_task(&var->mutex->waiting, signalled_task);
    
    // Both the current task and the signalled task are contending for the lock.
    var->mutex->owner_state = pack_owner_state(current_task, ACQUIRED_CONTENDED);
    
    qos_remove_dnode(&signalled_task->timeout_node);
  }

  return release_mutex_critical(scheduler, var->mutex);
}

void qos_release_and_signal_condition_var(ConditionVar* var) {
  assert(owns_mutex(var->mutex));
  qos_critical_section(release_and_signal_condition_var_critical, var);
}

static qos_task_state_t release_and_broadcast_condition_var_critical(Scheduler* scheduler, void* v) {
  auto var = (ConditionVar*) v;

  auto current_task = scheduler->current_task;

  while (!empty(begin(var->waiting))) {
    auto signalled_task = &*begin(var->waiting);

    // The current task owns the mutex so the signalled task is not immediately
    // ready. Rather it is moved from the condition variable's waiting list to
    // the mutex's.
    internal_insert_scheduled_task(&var->mutex->waiting, signalled_task);
    
    // Both the current task and one or more signalled tasks are contending for the lock.
    var->mutex->owner_state = pack_owner_state(current_task, ACQUIRED_CONTENDED);
    
    qos_remove_dnode(&signalled_task->timeout_node);
  }

  return release_mutex_critical(scheduler, var->mutex);
}

void qos_release_and_broadcast_condition_var(ConditionVar* var) {
  assert(owns_mutex(var->mutex));
  qos_critical_section(release_and_broadcast_condition_var_critical, var);
}
