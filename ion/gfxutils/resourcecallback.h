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

#ifndef ION_GFXUTILS_RESOURCECALLBACK_H_
#define ION_GFXUTILS_RESOURCECALLBACK_H_

#include <vector>

#include "ion/base/referent.h"
#include "ion/gfx/resourcemanager.h"
#include "ion/port/semaphore.h"

namespace ion {
namespace gfxutils {

// Class that allows blocking until a callback is called and automagically
// destroys itself after both Callback() and WaitForCompletion() have executed.
// This is accomplished by holding two references to itself.
template <typename T> class ResourceCallback : public base::Referent {
 public:
  using RefPtr = base::SharedPtr<ResourceCallback<T>>;
  ResourceCallback() {
    callback_holder_.Reset(this);
    wait_holder_.Reset(this);
  }

  // Returns the data received by Callback().
  const std::vector<T>& GetData() const { return data_; }

  // This function is compatible with ResourceManager::InfoCallback, and should
  // be used as the callback passed to ResourceManager's Request() functions. It
  // will be called after the request has been serviced and with the requested
  // data.
  void Callback(const std::vector<T>& data) {
    // Store the data.
    data_ = data;

    // Increment the ref count of this so that the member assignment can
    // succeed without calling the destructor.
    RefPtr holder(this);
    callback_holder_ = nullptr;

    // Signal that the callback has been called.
    semaphore_.Post();

    holder = nullptr;
    // This may have been deleted by the above assignment; do not use the
    // ResourceCallback after Callback() returns.
  }

  void WaitForCompletion(std::vector<T>* data) {
    // Wait for the callback to be called.
    semaphore_.Wait();

    // Store the data if we've waited for the callback (in which case
    // callback_holder_ is nullptr) and the caller has requested it.
    if (!callback_holder_.Get() && data)
      *data = data_;

    // Increment the ref count of this so that the member assignment can
    // succeed without calling the destructor.
    RefPtr holder(this);
    wait_holder_ = nullptr;
    holder = nullptr;
    // This may have been deleted by the above assignment; do not use the
    // ResourceCallback after WaitForCompletion() returns.
  }

 protected:
  // The constructor is protected because this class is derived from Referent.
  ~ResourceCallback() override {
    DCHECK(callback_holder_.Get() == nullptr);
    DCHECK(wait_holder_.Get() == nullptr);
  }

  std::vector<T> data_;
  port::Semaphore semaphore_;
  RefPtr callback_holder_;
  RefPtr wait_holder_;
};

typedef ResourceCallback<gfx::ResourceManager::ArrayInfo> ArrayCallback;
typedef ResourceCallback<gfx::ResourceManager::BufferInfo> BufferCallback;
typedef ResourceCallback<gfx::ResourceManager::FramebufferInfo>
    FramebufferCallback;
typedef ResourceCallback<gfx::ResourceManager::SamplerInfo> SamplerCallback;
typedef ResourceCallback<gfx::ResourceManager::ShaderInfo> ShaderCallback;
typedef ResourceCallback<gfx::ResourceManager::PlatformInfo> PlatformCallback;
typedef ResourceCallback<gfx::ResourceManager::ProgramInfo> ProgramCallback;
typedef ResourceCallback<gfx::ResourceManager::TextureImageInfo>
    TextureImageCallback;
typedef ResourceCallback<gfx::ResourceManager::TextureInfo> TextureCallback;

}  // namespace gfxutils
}  // namespace ion

#endif  // ION_GFXUTILS_RESOURCECALLBACK_H_
