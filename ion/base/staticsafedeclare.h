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

#ifndef ION_BASE_STATICSAFEDECLARE_H_
#define ION_BASE_STATICSAFEDECLARE_H_

#include <stdint.h>

#include <mutex>  // NOLINT(build/c++11)
#include <type_traits>
#include <vector>

#include "ion/base/shareable.h"
#include "ion/base/sharedptr.h"
#include "ion/base/static_assert.h"
#include "ion/port/atomic.h"

// Use the below ION_DECLARE_STATIC_* macros to safely initialize a local static
// pointer variable with 4- or 8-byte size. These macros will not work for
// global statics, and these macros are not necessary for POD (Plain Old
// Datatype) variables.
//
// For example, to safely create a function that returns a singleton instance:
//   MySingletonClass* GetSingleton() {
//     ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(
//         MySingletonClass, s_singleton, (new MySingletonClass(arg1, arg2)));
//     return s_singleton;
//   }
// The macro declares s_singleton with the right type and initializes it with
// the passed constructor.
//
// Some more examples:
//
//   // Declare foo_ptr as a pointer to a single int.
//   ION_DECLARE_SAFE_STATIC_POINTER(int, foo_ptr);
//   // Declare foo_array as an int* of 10 ints.
//   ION_DECLARE_SAFE_STATIC_ARRAY(int, foo_array, 10);
//   // Declare foo_struct as a pointer to MyStructDeletedSecond.
//   ION_DECLARE_SAFE_STATIC_POINTER(MyStructDeletedSecond, foo_struct);
//   // Declare foo_struct2 as a pointer to MyStructDeletedFirst, calling a
//   // non-default constructor. The constructor must be in parentheses for the
//   // macro to expand correctly.
//   ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(
//       MyStructDeletedFirst, foo_struct2, (new MyStructDeletedFirst(2)));
//
// All pointer-types initialized with ION_DECLARE_STATIC_* are added to a static
// vector of pointers through the StaticDeleter class, below. StaticDeleter is
// derived from Shareable so it can be managed by a SharedPtr. When it is
// destroyed as the program exits it cleans up any pointers that were added to
// it in the reverse order they were created. This ensures proper dependency
// handling.

// Uses AtomicCompareAndSwap to uniquely set the value of variable with
// new_variable. If the swap fails then destroys new_variable.
#define ION_SAFE_ASSIGN_STATIC_POINTER(                          \
    type, variable, new_variable, add_func, destroyer) \
  type null = nullptr;                                           \
  if (variable.compare_exchange_strong(null, new_variable)) {    \
    add_func(#type, new_variable);                               \
  } else {                                                       \
    destroyer new_variable;                                      \
  }

// Declares and thread-safely assigns the value of a static variable.
#define ION_DECLARE_SAFE_STATIC(                                         \
    type, variable, constructor, add_func, destroyer)                    \
  static std::atomic<type> atomic_##variable; /* Zero-initialized */     \
  ION_STATIC_ASSERT(std::is_pointer<type>::value,                        \
                    "static variables must be of pointer type");         \
  type variable = atomic_##variable.load(std::memory_order_acquire);     \
  if (variable == 0) {                                                   \
    type new_##variable = constructor;                                   \
    ION_SAFE_ASSIGN_STATIC_POINTER(                                      \
        type, atomic_##variable, new_##variable, add_func, destroyer);   \
    variable = atomic_##variable.load(std::memory_order_acquire);        \
  }

//------------------------------------------------------------------------------
//
// Use the below ION_DECLARE_STATIC to declare static variables.
//
//------------------------------------------------------------------------------

// Declares a static array variable.
#define ION_DECLARE_SAFE_STATIC_ARRAY(type, variable, count)            \
  ION_DECLARE_SAFE_STATIC(                                              \
      type*,                                                            \
      variable,                                                         \
      new type[count],                                                  \
      ion::base::StaticDeleterDeleter::GetInstance()->AddArrayToDelete, \
      delete[])

// Declare a static non-array pointer and calls a default constructor.
#define ION_DECLARE_SAFE_STATIC_POINTER(type, variable)                   \
  ION_DECLARE_SAFE_STATIC(                                                \
      type*,                                                              \
      variable,                                                           \
      new type,                                                           \
      ion::base::StaticDeleterDeleter::GetInstance()->AddPointerToDelete, \
      delete)

// Declare a static non-array pointer and calls a non-default constructor. The
// constructor parameter must be enclosed in parenthesis for the macro to expand
// correctly.
#define ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(                 \
    type, variable, constructor)                                          \
  ION_DECLARE_SAFE_STATIC(                                                \
      type*,                                                              \
      variable,                                                           \
      constructor,                                                        \
      ion::base::StaticDeleterDeleter::GetInstance()->AddPointerToDelete, \
      delete)

// Declare a static array variable with initialzer list.
#define ION_DECLARE_SAFE_STATIC_ARRAY_WITH_INITIALIZERS(type, variable, count, \
                                                        ...)                   \
  ION_DECLARE_SAFE_STATIC(                                                     \
      type*, variable, (new type[count]{__VA_ARGS__}),                         \
      ion::base::StaticDeleterDeleter::GetInstance()->AddArrayToDelete,        \
      delete[])

namespace ion {
namespace base {

class StaticDeleterBase {
 public:
  explicit StaticDeleterBase(const std::string& name) : type_name_(name) {}
  virtual ~StaticDeleterBase() {}

  // Returns the name of the type this deleter deletes.
  const std::string& GetTypeName() const { return type_name_; }

 protected:
  std::string type_name_;
};

// This class should not be used directly. Only use it through one of the above
// ION_DECLARE_STATIC_* defines.
//
// StaticDeleter is a convenience class that holds a static pointer that needs
// to be deleted at shutdown.
template <typename T> class StaticDeleter : public StaticDeleterBase {
 public:
  StaticDeleter(const std::string& name, T* pointer_to_delete)
      : StaticDeleterBase(name),
        pointer_to_delete_(pointer_to_delete) {}

 private:
  // The destructor is private because all base::Shareable classes must have
  // protected or private destructors.
  ~StaticDeleter() override {
    delete pointer_to_delete_;
  }

  // Vector of pointers to delete at destruction.
  T* pointer_to_delete_;
};

// Specialization for arrays.
template <typename T> class StaticDeleter<T[]> : public StaticDeleterBase {
 public:
  StaticDeleter(const std::string& name, T* pointer_to_delete)
      : StaticDeleterBase(name),
        pointer_to_delete_(pointer_to_delete) {}

 private:
  // The destructor is private because all base::Shareable classes must have
  // protected or private destructors.
  ~StaticDeleter() override {
    delete [] pointer_to_delete_;
  }

  // Vector of array pointers to delete at destruction.
  T* pointer_to_delete_;
};

// StaticDeleterDeleter is an internal class that holds and deletes
// StaticDeleters; it should not be used directly. Though it is an internal
// class, it must be decorated since it is required by StaticDeleters that may
// be instantiated in other translation units. The single StaticDeleterDeleter
// is destroyed at program shutdown, and destroys all StaticDeleters in the
// reverse order they were created with the above macros.
class ION_API StaticDeleterDeleter : public Shareable {
 public:
  StaticDeleterDeleter();
  ~StaticDeleterDeleter() override;

  // Adds a regular pointer to be deleted when this class is destroyed.
  template <typename T>
  void AddPointerToDelete(const std::string& name, T* ptr) {
    std::lock_guard<std::mutex> locker(mutex_);
    deleters_.push_back(new StaticDeleter<T>(name, ptr));
  }
  // Adds an array pointer to be deleted when this class is destroyed.
  template <typename T>
  void AddArrayToDelete(const std::string& name, T* ptr) {
    std::lock_guard<std::mutex> locker(mutex_);
    deleters_.push_back(new StaticDeleter<T[]>(name, ptr));
  }

  // Returns a pointer to the global instance.
  static StaticDeleterDeleter* GetInstance();

  // Returns the deleter at the passed index. Returns nullptr if the index is
  // invalid.
  const StaticDeleterBase* GetDeleterAt(size_t index) const {
    return index < deleters_.size() ? deleters_[index] : nullptr;
  }

  // Returns the number of deleters in this.
  size_t GetDeleterCount() const { return deleters_.size(); }

  // Call this function once, and only once, at the end of a program, to
  // explicitly destroy all StaticDeleters (including the StaticDeleterDeleter
  // instance). Any attempt to access pointers declared with the macros in this
  // file will fail after this call. Even if this function is never called,
  // however, the static instance will still be destroyed at program shutdown,
  // it will just probably happen after any memory trackers are destroyed,
  // causing them to report erroneous leaks.
  static void DestroyInstance();

 private:
  // Needed for construction order and thread safety--see impl notes.
  // Must only be called from GetInstance(). The string parameter is
  // ignored but needed for consistency with AddPointerToDelete.
  static void SetInstancePtr(const std::string&,
                             StaticDeleterDeleter* instance);

  std::vector<StaticDeleterBase*> deleters_;

  // Mutex to lock the pointer vector.
  std::mutex mutex_;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_STATICSAFEDECLARE_H_
