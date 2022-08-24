#include "semaphore.h"
#include "semaphore.internal.h"

#include "atomic.h"
#include "critical.inl.c"
#include "dlist_it.h"
#include "sync.internal.h"

#include <cassert>

Semaphore* new_semaphore(int32_t initial_count) {
  auto semaphore = new Semaphore;
  init_semaphore(semaphore, initial_count);
  return semaphore;
}

void init_semaphore(Semaphore* semaphore, int32_t initial_count) {
  assert(initial_count >= 0);
  semaphore->count = initial_count;
  init_dlist(&semaphore->waiting.tasks);
}

static TaskState STRIPED_RAM acquire_semaphore_critical(va_list args) {
  void internal_insert_delayed_task(Task* task, int32_t quanta);

  auto semaphore = va_arg(args, Semaphore*);
  auto count = va_arg(args, int32_t);
  auto timeout = va_arg(args, int32_t);

  auto old_count = semaphore->count;
  auto new_count = old_count - count;
  if (new_count >= 0) {
    semaphore->count = new_count;
    critical_set_current_critical_section_result(true);
    return TASK_RUNNING;
  }

  if (timeout == 0) {
    return TASK_RUNNING;
  }

  // Insert current task into linked list, maintaining descending priority order.
  internal_insert_sync_list(&semaphore->waiting, current_task);
  current_task->sync_state = count;

  if (timeout > 0) {
    internal_insert_delayed_task(current_task, timeout);
  }

  return TASK_SYNC_BLOCKED;
}

bool STRIPED_RAM acquire_semaphore(Semaphore* semaphore, int32_t count, int32_t timeout) {
  assert(count >= 0);

  auto old_count = semaphore->count;
  auto new_count = old_count - count;
  if (new_count >= 0 && atomic_compare_and_set(&semaphore->count, old_count, new_count) == old_count) {
    return true;
  }

  if (timeout == 0) {
    return false;
  }

  return critical_section_va(acquire_semaphore_critical, semaphore, count, timeout);
}

TaskState STRIPED_RAM release_semaphore_critical(va_list args) {
  auto semaphore = va_arg(args, Semaphore*);
  auto count = va_arg(args, int32_t);

  semaphore->count += count;

  auto should_yield = false;
  auto position = begin(semaphore->waiting);
  while (position != end(semaphore->waiting)) {
    auto task = &*position;

    if (task->sync_state <= semaphore->count) {
      semaphore->count -= task->sync_state;

      position = remove(position);

      critical_set_critical_section_result(task, true);
      should_yield |= critical_ready_task(task);
    } else {
      ++position;
    }
  }

  if (should_yield) {
    return TASK_READY;
  } else {
    return TASK_RUNNING;
  }
}

void STRIPED_RAM release_semaphore(Semaphore* semaphore, int32_t count) {
  assert(count >= 0);
  critical_section_va(release_semaphore_critical, semaphore, count);
}
