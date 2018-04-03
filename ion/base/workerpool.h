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

#ifndef ION_BASE_WORKERPOOL_H_
#define ION_BASE_WORKERPOOL_H_

#include <atomic>
#include <functional>
#include <mutex>   // NOLINT(build/c++11)
#include <thread>  // NOLINT(build/c++11)

#include "ion/base/stlalloc/allocvector.h"
#include "ion/port/semaphore.h"

namespace ion {
namespace base {

// Manages one or more threads that run in a loop, performing some work with
// each iteration (if any work is available).
//
// Newly-constructed WorkerPools are "suspended"... Resume() must be called
// in order for work to commence.  This allows the user of this code to
// guaranteed that the "worker" object has been fully-constructed by the
// time that Resume() is called.  For example, an anticipated use-case is
// to have an class which:
//   - encapsulates a WorkerPool
//   - implements Worker
//   - passes itself via "this" to the WorkerPool's constructor.
// If the DoWork() method is implemented in a subclass, undefined behavior will
// result if a thread calls DoWork() before all constructors finish.  Similarly,
// Suspend() must be called before the subclass destructor is invoked.
class WorkerPool : public ion::base::Allocatable {
 public:
  // Interface to enable pluggable worker behavior.  One of the WorkerPool
  // threads will invoke Worker::DoWork() when there is work available to do
  // (i.e. when someone calls GetWorkSemaphore()->Post()).
  class Worker {
   public:
    // Called repeatedly in worker thread loop, whenever GetWorkSemaphore() is
    // signaled to indicate that there is work to do.
    //
    // Note: the WorkerPool implementation occasionally signals the semaphore
    // during state changes (e.g., adding/removing threads, and when suspending
    // or resuming); subclasses of Worker must handle this gracefully.
    // However, there won't be many of these "extra" signals, so DoWork() can
    // simply return as soon as it realizes there is no work to do.
    // In particular, Worker subclasses *should not* try to proactively limit
    // CPU use, e.g. by sleeping for a few milliseconds when no work is
    // available.
    virtual void DoWork() = 0;

    // Return the worker's name.
    virtual const std::string& GetName() const = 0;

   protected:
    // Virtual classes need virtual desructors.
    virtual ~Worker() {}
  };

  // Constructor/destructor.
  explicit WorkerPool(Worker* worker);
  ~WorkerPool() override;

  // Gets a descriptive name for the pool from the worker.
  const std::string& GetName() const { return worker_->GetName(); }

  // Suspends all threads until Resume() is called.  Wait for each thread to
  // finish what they're doing and acknowledge the suspend-request.
  void Suspend();
  // Resumes all threads.
  void Resume();
  // Return true if pool's threads are suspended.
  bool IsSuspended() const;

  // Changes the number of theads in the pool.
  void ResizeThreadPool(size_t thread_count);

  // Returns the semaphore that is used to signal that a unit of work is
  // available to process.
  port::Semaphore* GetWorkSemaphore() { return &work_sema_; }

 protected:
  // ThreadEntryPoint() is run on each created thread.  Subclasses may override
  // this method, but they must call the superclass implementation.
  //
  // IMPORTANT: subclasses which override ThreadEntryPoint() MUST call
  // ResizeThreadPool(0) in their destructor to ensure that the vtable isn't
  // changed while there exist active invocations of ThreadEntryPoint().
  virtual void ThreadEntryPoint();

 private:
  // Tells all threads to quit, and wait for them to finish.
  void KillAllThreads();

  // Error-catching wrappers around Wait() and Post().
  static void Wait(port::Semaphore* sema);
  static void Post(port::Semaphore* sema);

  Worker* const worker_;
  ion::base::AllocVector<std::thread> threads_;
  port::Semaphore work_sema_;
  port::Semaphore active_threads_sema_;
  std::atomic<bool> suspended_;
  // Set to true during KillAllThreads().  When ThreadEntryPoint() notices that
  // this is true, it exits immediately.
  std::atomic<bool> killing_;
  // Set to true when some configuration change is occurring (e.g. suspending,
  // resuming, or killing threads).  This results in low overhead in the common
  // case: a single atomic read allows ThreadEntryPoint() to decide whether to
  // enter the "slow path" logic.
  std::atomic<bool> slow_path_;
  std::function<bool()> spawn_func_;
  mutable std::mutex mutex_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WorkerPool);
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_WORKERPOOL_H_
