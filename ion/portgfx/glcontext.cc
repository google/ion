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

#include "ion/portgfx/glcontext.h"

#include <stdio.h>  // for snprintf

#include <atomic>
#include <cstring>
#include <mutex>  // NOLINT(build/c++11)
#include <unordered_map>
#include <utility>

#include "ion/base/allocatable.h"
#include "ion/base/lockguards.h"
#include "ion/base/logging.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/base/threadlocalobject.h"
#include "ion/portgfx/glheaders.h"

namespace ion {
namespace portgfx {
namespace {

// 2 characters per byte, a potential 0x prefix and a null terminator.
const int kDummySourceLength = 2 * sizeof(GlContext*) + 5;

// The StaticGlContextData class holds:
//
// * The singleton mapping of OpenGL context IDs to GlContext instances.
//   As this is global state, the mapping is queried and modified through static
//   functions.
// * The thread-local pointer to the current GlContext on the thread.
//
// Note that StaticGlContextData holds base::Allocatable members and is itself
// accessed as a singleton instance.  Thus it is itself is derived from
// base::Allocatable to ensure correct destruction order of the instance with
// respect to the Allocatable framework.
class StaticGlContextData : public base::Allocatable {
 public:
  // Finds a GlContext in the singleton mapping from GL context IDs to
  // GlContexts. Returns nullptr if not found.
  static GlContextPtr FindInGlContextMap(uintptr_t gl_context_id) {
    DCHECK_NE(0, gl_context_id);
    GlContextPtr context;
    StaticGlContextData* const instance = GetInstance();
    std::lock_guard<std::mutex> lock(instance->context_map_mutex_);
    auto iter = instance->context_map_.find(gl_context_id);
    if (iter != instance->context_map_.end()) {
      context = iter->second.Acquire();
      // If the GlContext exists in the map, its destructor has not yet been
      // called.
      DCHECK(context);
    }
    return context;
  }

  // Inserts a GlContext into the singleton mapping from GL context IDs to
  // GlContexts.
  static void InsertIntoGlContextMap(uintptr_t gl_context_id,
                                     const GlContextPtr& context) {
    DCHECK_NE(0, gl_context_id);
    StaticGlContextData* const instance = GetInstance();
    std::lock_guard<std::mutex> lock(instance->context_map_mutex_);
    if (instance->context_map_.erase(gl_context_id)) {
      // This may happen if the GL context is managed outside of Ion and a
      // context address is reused.
      LOG(INFO) << "Overwriting GlContext for GL context ID " << gl_context_id;
    }
    auto result = instance->context_map_.insert(
        {gl_context_id, base::WeakReferentPtr<GlContext>(context)});
    DCHECK(result.second);
    // The DCHECK above will compile to nothing in prod builds, so we add this
    // statement just in case the compiler decides to complain about an unused
    // local variable.
    (void)result;
  }

  // Erases a GlContext from the singleton mapping from GL context IDs to
  // GlContexts.
  static void EraseFromGlContextMap(uintptr_t gl_context_id) {
    DCHECK_NE(0, gl_context_id);
    StaticGlContextData* const instance = GetInstance();
    std::lock_guard<std::mutex> lock(instance->context_map_mutex_);
    const size_t erased = instance->context_map_.erase(gl_context_id);
    DCHECK_GE(1U, erased);
  }

  // Returns the singleton thread-local GlContextPtr holding a reference to the
  // thread's current GlContext.
  static GlContextPtr* GetThreadCurrentGlContextPtr() {
    return GetInstance()->thread_current_context_.Get();
  }

 private:
  class GlContextMap
      : public std::unordered_map<uintptr_t, base::WeakReferentPtr<GlContext>> {
   public:
    ~GlContextMap() {
      // On iOS we ignore this DCHECK rather than crash, because it
      // allows end-to-end tests to pass. See b/30750983.
#ifndef ION_PLATFORM_IOS
      DCHECK(empty())
          << "Undestroyed GlContext objects detected at program exit. "
             "This usually indicates a memory leak of an object that contains "
             "a GlContextPtr or a RendererPtr.";
#endif
    }
  };

  // Returns the singleton StaticGlContextData instance.
  static StaticGlContextData* GetInstance() {
    ION_DECLARE_SAFE_STATIC_POINTER(StaticGlContextData, s_helper);
    return s_helper;
  }

  // The Mutex protecting |context_map_|.
  std::mutex context_map_mutex_;

  // The map of GlContext context IDs to GlContext instances.
  GlContextMap context_map_;

  // The thread-local current GlContext pointer.  Note that this object holds a
  // reference to to the GlContext, so a GlContext is never destroyed before it
  // is made not current on all threads.
  //
  // Note also that it is important that |context_map_| outlives
  // |thread_current_context_|, as GlContext removes itself from |context_map_|
  // during destruction, and |thread_current_context_| holds a reference to the
  // GlContext.  This is ensured here by declaring |thread_current_context_|
  // after |context_map_|.
  base::ThreadLocalObject<GlContextPtr> thread_current_context_;
};

}  // namespace

GlContext::GlContext()
    : id_(0), share_group_id_(0), gl_context_id_(0), dummy_shader_id_(0) {}

GlContext::~GlContext() {
  if (gl_context_id_ != 0) {
    StaticGlContextData::EraseFromGlContextMap(gl_context_id_);
  }
}

uintptr_t GlContext::GetId() const { return id_; }

uintptr_t GlContext::GetShareGroupId() const { return share_group_id_; }

void GlContext::SetShareGroupId(uintptr_t group) {
  if (IsOwned()) {
    LOG(ERROR) << "SetShareGroupId can only be called on wrapped contexts.";
  } else {
    share_group_id_ = group;
  }
}

bool GlContext::DoesCurrentContextMatch() const {
  if (dummy_shader_id_ == 0) return false;
  return CheckStamp();
}

// static
GlContextPtr GlContext::GetCurrent(bool check_stamp) {
  GlContextPtr* const thread_current_context_ptr =
      StaticGlContextData::GetThreadCurrentGlContextPtr();
  GlContextPtr new_thread_current_context;

  // Reset the current wrapper, if it does not match the current GL context ID.
  // By checking IsCurrentGlContext() on the current GlContext (if it exists)
  // before using GetCurrentGlContextId(), this allows GlContexts which consider
  // themselves to be current even without a current underlying platform context
  // to be returned by GetCurrent() -- important e.g. in the case of
  // FakeGlContext.
  if (*thread_current_context_ptr) {
    // Reset the current wrapper, if it does not match the GL context ID.
    if ((*thread_current_context_ptr)->IsCurrentGlContext()) {
      if (!check_stamp || (*thread_current_context_ptr)->CheckStamp()) {
        // The GlContext in |thread_current_context_ptr| is current.
        new_thread_current_context = *thread_current_context_ptr;
      } else {
        // Context was changed to a different (new) context that still has the
        // same address, but does not contain the GL objects we expect.
        // Reset the context ID, so calling MakeCurrent() on any other remaining
        // reference to that GlContext will return an error.
        (*thread_current_context_ptr)->gl_context_id_ = 0;
      }
    } else {
      // No-op.  |new_thread_current_context| remains nullptr.
    }
  }
  const uintptr_t current_gl_context_id = GetCurrentGlContextId();
  if (current_gl_context_id != 0) {
    // If there is a current OpenGL context, there should exist a GlContext
    // managing it.  Find it, or create one.
    new_thread_current_context =
        StaticGlContextData::FindInGlContextMap(current_gl_context_id);
    if (new_thread_current_context) {
      // Check whether we have the correct context.
      if (check_stamp && !new_thread_current_context->CheckStamp()) {
        new_thread_current_context->gl_context_id_ = 0;
        new_thread_current_context.Reset();
      }
    }
    if (!new_thread_current_context) {
      // No wrapper managing this context exists, so create a new one.
      new_thread_current_context = CreateWrappingGlContext();

      // CreateWrappingGlContext() should have added an entry into the context
      // map.
      DCHECK_EQ(
          new_thread_current_context.Get(),
          StaticGlContextData::FindInGlContextMap(current_gl_context_id).Get());
    }
  }

  // We defer assigning to |thread_current_context_ptr| until the very end,
  // outside of any locks taken, as the assignment may require releasing the old
  // value (which may take locks while running the GlContext destructor).
  *thread_current_context_ptr = new_thread_current_context;
  if (*thread_current_context_ptr) {
    (*thread_current_context_ptr)->MaybeCreateStamp();
  }
  return *thread_current_context_ptr;
}

// static
bool GlContext::MakeCurrent(const GlContextPtr& context) {
  GlContextPtr current_context = GetCurrent();
  if (context == current_context) {
    // Return early if we are already current.
    return true;
  }

  if (current_context) {
    // Clear the current context, if there was one.
    current_context->ClearCurrentContextImpl();
  }

  GlContextPtr* const thread_current_context_ptr =
      StaticGlContextData::GetThreadCurrentGlContextPtr();
  thread_current_context_ptr->Reset();

  if (context) {
    if (context->gl_context_id_ == 0 || !context->MakeContextCurrentImpl()) {
      LOG(ERROR) << "Failed to make context current.";
      return false;
    }
    *thread_current_context_ptr = context;
  }

  if (*thread_current_context_ptr) {
    (*thread_current_context_ptr)->MaybeCreateStamp();
  }
  return true;
}

void GlContext::MaybeCreateStamp() {
  // The stamp is implemented as a shader object that stores the address of the
  // wrapping GlContext object in its source.
  typedef GLuint(ION_APIENTRY * CreateShaderFn)(GLenum);
  typedef void(ION_APIENTRY * ShaderSourceFn)(GLuint, GLsizei, const GLchar**,
                                              const GLint*);
  static auto s_create_shader = reinterpret_cast<CreateShaderFn>(
      GetProcAddress("glCreateShader", kProcAddressCore | kProcAddressPure));
  static auto s_shader_source = reinterpret_cast<ShaderSourceFn>(
      GetProcAddress("glShaderSource", kProcAddressCore | kProcAddressPure));

  // Return early if the dummy shader was already created.
  if (dummy_shader_id_ != 0) return;
  if (s_create_shader == nullptr || s_shader_source == nullptr) {
    LOG_ONCE(WARNING) << "GL functions not found, dummy shaders disabled";
    return;
  }
  DCHECK_EQ(GetCurrentGlContextId(), gl_context_id_);

  // Store this GlContext's address in a dummy shader object. On most platforms,
  // the shader source interface can be used to store completely arbitrary
  // binary data, but on some, only ASCII data is allowed, and  a newline might
  // be appended if there isn't one at the end of the source.
  char dummy_source[kDummySourceLength];
  snprintf(dummy_source, sizeof dummy_source, "%p\n", this);
  const char* dummy_source_ptr = dummy_source;
  const GLint dummy_length = static_cast<GLint>(strlen(dummy_source));
  dummy_shader_id_ = s_create_shader(GL_FRAGMENT_SHADER);
  s_shader_source(dummy_shader_id_, 1, &dummy_source_ptr, &dummy_length);
}

bool GlContext::CheckStamp() const {
  typedef GLboolean(ION_APIENTRY * IsShaderFn)(GLuint);
  typedef void(ION_APIENTRY * GetShaderSourceFn)(GLuint, GLsizei, GLsizei*,
                                                 GLchar*);
  static auto s_is_shader = reinterpret_cast<IsShaderFn>(
      GetProcAddress("glIsShader", kProcAddressCore | kProcAddressPure));
  static auto s_get_shader_source = reinterpret_cast<GetShaderSourceFn>(
      GetProcAddress("glGetShaderSource", kProcAddressCore | kProcAddressPure));

  if (dummy_shader_id_ == 0) return true;
  // If glCreateShader is not available, the things below will never execute,
  // since dummy_shader_id_ will always be zero.
  if (!s_is_shader(dummy_shader_id_)) return false;
  // Check the shader's source against the expected value.
  char expected_source[kDummySourceLength];
  char actual_source[kDummySourceLength];
  snprintf(expected_source, sizeof expected_source, "%p\n", this);
  GLsizei dummy_length = 0;
  s_get_shader_source(dummy_shader_id_, sizeof actual_source, &dummy_length,
                      actual_source);
  return strncmp(expected_source, actual_source, kDummySourceLength) == 0;
}

// static
uintptr_t GlContext::GetCurrentId() {
  const GlContextPtr current = GetCurrent();
  if (!current) {
    return 0;
  }
  return current->GetId();
}

// static
GlContextPtr GlContext::CreateGlContextInCurrentShareGroup(
    const GlContextSpec& spec) {
  const GlContextPtr current = GetCurrent();
  if (!current) {
    return GlContextPtr();
  }
  return current->CreateGlContextInShareGroupImpl(spec);
}

// static
void GlContext::RefreshCurrentGlContext() {
  const GlContextPtr current = GetCurrent();
  if (!current) {
    return;
  }
  current->RefreshGlContextImpl();
}

void GlContext::RefreshGlContextImpl() {}

void GlContext::CleanupThreadImpl() const {}

bool GlContext::IsCurrentGlContext() const {
  return GetGlContextId() == GetCurrentGlContextId();
}

void GlContext::SetIds(uintptr_t id, uintptr_t share_group_id,
                       uintptr_t gl_context_id) {
  DCHECK_EQ(0, id_);
  DCHECK_EQ(0, share_group_id_);
  DCHECK_EQ(0, gl_context_id_);
  id_ = id;
  share_group_id_ = share_group_id;
  gl_context_id_ = gl_context_id;

  // No context should exist without a |gl_context_id_|.
  DCHECK_NE(0, gl_context_id_);

  // Since we insert |this| into the context map as a weak pointer, |this| must
  // already be held in a SharedPtr; i.e. SetIds() cannot be called directly
  // from a constructor.
  DCHECK_LT(0, this->GetRefCount())
      << "|this| not held in a SharedPtr "
         "(is GlContext::SetIds() being called directly from a constructor?)";
  if (gl_context_id_ != 0) {
    // Only attempt to insert into the context map if |gl_context_id_| is
    // non-zero.  (It should only be zero for the special case of the
    // FakeGlContext.)
    StaticGlContextData::InsertIntoGlContextMap(gl_context_id_,
                                                GlContextPtr(this));
  }
}

uintptr_t GlContext::GetGlContextId() const { return gl_context_id_; }

// static
uintptr_t GlContext::CreateId() {
  static std::atomic<uintptr_t> next_id(1);
  return next_id++;
}

// static
uintptr_t GlContext::CreateShareGroupId() {
  static std::atomic<uintptr_t> next_share_group_id(1);
  return next_share_group_id++;
}

// static
void GlContext::CleanupThread() {
  // Ion holds a thread-local SharedPtr to the current GlContext, so a GlContext
  // can never be destroyed unless it is detached from all threads.
  GlContextPtr current_context = GetCurrent();
  MakeCurrent(GlContextPtr());
  if (current_context) {
    current_context->CleanupThreadImpl();
  }
}

}  // namespace portgfx
}  // namespace ion
