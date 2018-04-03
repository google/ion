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

#ifndef ION_BASE_SIGNAL_H_
#define ION_BASE_SIGNAL_H_

// Type-safe, thread-safe callback list mechanism modeled after libsigc++ and
// Boost.Signal.

#include <functional>
#include <list>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)

#include "ion/port/useresult.h"

namespace ion {
namespace base {

// The generic template is empty, only the specialization for function types
// with void return is fully defined.
template <typename Function> class Signal {};

// Type-erased class representing an association between a signal and a slot.
// After calling Disconnect() or destroying this object, the slot will no longer
// be invoked on signal emission.
class Connection {
 public:
  Connection() {}
  void Disconnect() { entry_.reset(); }

  // Detaches the slot from the connection object. Once this method is called,
  // destruction of the Connection object will not cause the associated slot to
  // be disconnected. It will keep being called on every emission until the
  // signal is destroyed.
  void Detach();

 private:
  struct SlotEntry {
    // Technically this could be a pure virtual destructor, but that leads to
    // linker errors in ~ConcreteSlotEntry() about undefined ~SlotEntry().
    virtual ~SlotEntry() {}
    virtual void Detach() = 0;
  };
  explicit Connection(SlotEntry* entry) : entry_(entry) {}
  std::unique_ptr<SlotEntry> entry_;

  // Allow any Signal class to call the private constructor.
  template <typename> friend class Signal;
};

// Type-safe callback list. A Signal can be connected to any number of slots,
// which are all invoked when the Emit() function is called on the Signal.
// Currently, only functions with void return type are supported.
template <typename... Args>
class Signal<void(Args...)> {
 public:
  // Type of the object used as the slot (callback).
  using Slot = std::function<void(Args...)>;

  Signal() : data_(std::make_shared<SignalData>()) {}

  // Connects a slot to be invoked when the signal is emitted.
  ION_USE_RESULT Connection Connect(const Slot& slot) {
    std::lock_guard<std::mutex> guard(data_->mutex);
    auto slot_it = data_->slots.insert(data_->slots.end(), slot);
    return Connection(new ConcreteSlotEntry(data_, slot_it));
  }
  ION_USE_RESULT Connection Connect(Slot&& slot) {
    std::lock_guard<std::mutex> guard(data_->mutex);
    data_->slots.push_back(std::move(slot));
    return Connection(new ConcreteSlotEntry(data_, --data_->slots.end()));
  }

  // Emits a signal, invoking all registered slots.
  void Emit(Args&&... args) const {
    std::lock_guard<std::mutex> guard(data_->mutex);
    for (auto& slot : data_->slots) {
      slot(std::forward<Args>(args)...);
    }
  }

  // Emits a signal, invoking all registered slots. This variant also works when
  // the emission may connect and disconnect slots from this signal.
  void SafeEmit(Args&&... args) const {
    // We recursively invoke EmitInternal() to essentially copy the slots to the
    // stack and then invoke them before returning from the recursive function.
    // This maintains slot order and also allows disconnecting from the slot
    // functions.
    std::unique_lock<std::mutex> lock(data_->mutex);
    SafeEmitInternal(std::move(lock), data_->slots.rbegin(),
                     std::forward<Args>(args)...);
  }

 private:
  using SlotList = std::list<Slot>;
  using Iterator = typename SlotList::iterator;
  using ReverseIterator = typename SlotList::reverse_iterator;

  // Internal data of a signal. Accessed via shared_ptr for thread safety.
  struct SignalData {
    SlotList slots;
    std::mutex mutex;
  };

  // The concrete, function type specific implementation of slot disconnection.
  class ConcreteSlotEntry : public Connection::SlotEntry {
   public:
    ConcreteSlotEntry(const std::shared_ptr<SignalData>& data,
                      Iterator iterator)
        : data_(data), iterator_(std::move(iterator)) {}
    ~ConcreteSlotEntry() override {
      std::shared_ptr<SignalData> strong_data = data_.lock();
      if (strong_data) {
        std::lock_guard<std::mutex> guard(strong_data->mutex);
        strong_data->slots.erase(iterator_);
      }
    }
    void Detach() override { data_.reset(); }

   private:
    std::weak_ptr<SignalData> data_;
    Iterator iterator_;
  };

  void SafeEmitInternal(std::unique_lock<std::mutex> lock,
                        ReverseIterator iterator, Args&&... args) const {
    if (iterator == data_->slots.rend()) {
      lock.unlock();
      return;
    }
    Slot copied_slot = *iterator;
    SafeEmitInternal(std::move(lock), ++iterator, std::forward<Args>(args)...);
    copied_slot(std::forward<Args>(args)...);
  }

  std::shared_ptr<SignalData> data_;
};

inline void Connection::Detach() {
  entry_->Detach();
  entry_.reset();
}

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_SIGNAL_H_
