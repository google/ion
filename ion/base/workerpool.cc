/**
Copyright 2017 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#include "ion/base/workerpool.h"

#include "ion/base/lockguards.h"

namespace ion {
namespace base {

WorkerPool::WorkerPool(WorkerPool::Worker* worker)
    : worker_(CHECK_NOTNULL(worker)),
      threads_(GetNonNullAllocator()),
      suspended_(true),
      killing_(false),
      slow_path_(false),
      spawn_func_([this](){ this->ThreadEntryPoint(); return true; }) {}

WorkerPool::~WorkerPool() {
  ion::base::LockGuard lock(&mutex_);
  KillAllThreads();
}

void WorkerPool::ResizeThreadPool(size_t thread_count) {
  ion::base::LockGuard lock(&mutex_);

  // Check if we need to shrink the thread-pool.
  if (thread_count < threads_.size()) {
    // It's tricky to make the thread-pool smaller, because the current
    // implementation doesn't make it easy to pick a specific thread to kill.
    // Instead, we kill them all.
    KillAllThreads();
  }

  // Grow the pool to the desired size, one thread at a time.
  while (thread_count > threads_.size()) {
    threads_.insert(port::SpawnThreadStd(&spawn_func_));
    if (!suspended_)
      Post(&active_threads_sema_);
  }
}

void WorkerPool::Suspend() {
  ion::base::LockGuard lock(&mutex_);
  if (!suspended_) {
    suspended_ = true;
    // Grab all slots in active_threads_sema_ to prevent any of them from
    // running once this completes.
    slow_path_ = true;
    for (size_t i = 0; i < threads_.size(); ++i) {
      Wait(&active_threads_sema_);
    }
    slow_path_ = false;
  }
}

void WorkerPool::Resume() {
  ion::base::LockGuard lock(&mutex_);
  if (suspended_) {
    suspended_ = false;
    // Signal all slots in active_threads_sema_ to allow all threads to run.
    for (size_t i = 0; i < threads_.size(); ++i) {
      Post(&active_threads_sema_);
    }
  }
}

bool WorkerPool::IsSuspended() const {
  ion::base::LockGuard lock(&mutex_);
  return suspended_;
}

void WorkerPool::ThreadEntryPoint() {
  if (port::IsThreadNamingSupported())
    port::SetThreadName(GetName());

  while (true) {
    while (slow_path_) {
      // Typically we don't end up on the slow path; when we are, we must decide
      // what to do next.
      if (killing_) {
        // KillAllThreads() is being invoked on another thread.  Exit this
        // thread to allow it to join with the other thread.
        return;
      } else {
        // If |killing_| is false, we know that Suspend() is being invoked on
        // another thread.  Wait for it to complete.
        port::YieldThread();
      }
    }

    // Wait for someone (e.g. a JobQueue that has just enqueued a Job) to
    // signal the work-semaphore to wake up this thread.
    Wait(&work_sema_);
    Wait(&active_threads_sema_);
    worker_->DoWork();
    Post(&active_threads_sema_);
  }
}

void WorkerPool::KillAllThreads() {
  DCHECK(mutex_.IsLocked());

  // Set quit flag and signal worker threads to quit (once per thread).
  // Signal both semaphores to ensure that they run.
  killing_ = true;
  slow_path_ = true;
  for (size_t i = 0; i < threads_.size(); ++i) {
    // Ensure that threads aren't blocked on either Semaphore.
    Post(&work_sema_);
    Post(&active_threads_sema_);
  }

  // Wait for all threads to finish.
  for (const auto& thread_id : threads_) {
    bool join_succeeded = port::JoinThread(thread_id);
    DCHECK(join_succeeded);
  }
  threads_.clear();
  slow_path_ = false;
  killing_ = false;

  // Consume excess signals in |active_threads_sema_| only; not necessary for
  // |work_sema_| because DoWork() must be designed to handle extraneous calls.
  while (active_threads_sema_.TryWait()) {}
}

void WorkerPool::Wait(port::Semaphore* sema) {
  bool semaphore_wait_succeeded = sema->Wait();
  DCHECK(semaphore_wait_succeeded);
}

void WorkerPool::Post(port::Semaphore* sema) {
  bool semaphore_post_succeeded = sema->Post();
  DCHECK(semaphore_post_succeeded);
}

}  // namespace base
}  // namespace ion
