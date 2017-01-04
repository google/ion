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

#include "ion/portgfx/visual.h"

#include <atomic>
#include <cstring>
#include <unordered_map>
#include <utility>

#include "ion/base/allocatable.h"
#include "ion/base/lockguards.h"
#include "ion/base/logging.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/base/threadlocalobject.h"
#include "ion/port/mutex.h"
#include "ion/portgfx/glheaders.h"

namespace ion {
namespace portgfx {
namespace {

// The StaticVisualData class holds:
//
// * The singleton mapping of OpenGL context IDs to Visual instances.  As this
//   is global state, the mapping is queried and modified through static
//   functions.
// * The thread-local pointer to the current Visual on the thread.
//
// Note that StaticVisualData holds base::Allocatable members and is itself
// accessed as a singleton instance.  Thus it is itself is derived from
// base::Allocatable to ensure correct destruction order of the instance with
// respect to the Allocatable framework.
class StaticVisualData : public base::Allocatable {
 public:
  // Finds a Visual in the singleton mapping from GL context IDs to Visuals.
  // Returns nullptr if not found.
  static VisualPtr FindInVisualMap(uintptr_t gl_context_id) {
    DCHECK_NE(0, gl_context_id);
    VisualPtr visual;
    StaticVisualData* const instance = GetInstance();
    base::LockGuard lock(&instance->visual_map_mutex_);
    auto iter = instance->visual_map_.find(gl_context_id);
    if (iter != instance->visual_map_.end()) {
      visual = iter->second.Acquire();
      // If the Visual exists in the map, its destructor has not yet been
      // called.
      DCHECK(visual);
    }
    return visual;
  }

  // Inserts a Visual into the singleton mapping from GL context IDs to Visuals.
  // Returns true iff the Visual was successfully inserted (no existing Visual
  // has the same mapping).
  static bool InsertIntoVisualMap(uintptr_t gl_context_id,
                                  const VisualPtr& visual) {
    DCHECK_NE(0, gl_context_id);
    StaticVisualData* const instance = GetInstance();
    base::LockGuard lock(&instance->visual_map_mutex_);
    return instance->visual_map_
        .insert({gl_context_id, base::WeakReferentPtr<Visual>(visual)})
        .second;
  }

  // Erases a Visual from the singleton mapping from GL context IDs to Visuals.
  // Returns true iff a Visual was erased from the mapping.
  static bool EraseFromVisualMap(uintptr_t gl_context_id) {
    DCHECK_NE(0, gl_context_id);
    StaticVisualData* const instance = GetInstance();
    base::LockGuard lock(&instance->visual_map_mutex_);
    const size_t erased = instance->visual_map_.erase(gl_context_id);
    DCHECK_GE(1U, erased);
    return erased;
  }

  // Returns the singleton thread-local VisualPtr holding a reference to the
  // thread's current Visual.
  static VisualPtr* GetThreadCurrentVisualPtr() {
    return GetInstance()->thread_current_visual_.Get();
  }

 private:
  class VisualMap
      : public std::unordered_map<uintptr_t, base::WeakReferentPtr<Visual>> {
   public:
    ~VisualMap() {
#ifdef ION_PLATFORM_IOS
      // On iOS we warn about memory leak rather than crash, because it
      // allows end-to-end tests to pass. See bug.
      if (!empty()) {
        LOG(WARNING) << "VisualMap not empty";
      }
#else
      DCHECK(empty());
#endif
    }
  };

  // Returns the singleton StaticVisualData instance.
  static StaticVisualData* GetInstance() {
    ION_DECLARE_SAFE_STATIC_POINTER(StaticVisualData, s_helper);
    return s_helper;
  }

  // The Mutex protecting |visual_map_|.
  port::Mutex visual_map_mutex_;

  // The map of Visual context IDs to Visual instances.
  VisualMap visual_map_;

  // The thread-local current Visual pointer.  Note that this object holds a
  // reference to to the Visual, so a Visual is never destroyed before it is
  // made not current on all threads.
  //
  // Note also that it is important that |visual_map_| outlives
  // |thread_current_visual_|, as Visual removes itself from |visual_map_|
  // during destruction, and |thread_current_visual_| holds a reference to the
  // Visual.  This is ensured here by declaring |thread_current_visual_| after
  // |visual_map_|.
  base::ThreadLocalObject<VisualPtr> thread_current_visual_;
};

}  // namespace

Visual::Visual() : id_(0), share_group_id_(0), gl_context_id_(0) {}

Visual::~Visual() {
  if (gl_context_id_ != 0) {
    const bool erased = StaticVisualData::EraseFromVisualMap(gl_context_id_);
    DCHECK(erased);
  }
}

uintptr_t Visual::GetId() const { return id_; }

uintptr_t Visual::GetShareGroupId() const { return share_group_id_; }

void Visual::SetShareGroup(uintptr_t group) {
  if (IsOwned()) {
    LOG(ERROR) << "SetShareGroup can only be called on wrapped contexts.";
  } else {
    share_group_id_ = group;
  }
}

int Visual::GetGlVersion() const {
  DCHECK_EQ(this, GetCurrent().Get());

  // glGetIntegerv(GL_MAJOR_VERSION) is not part of core until OpenGL 3.0.
  const char* const version_string =
      reinterpret_cast<const char* const>(glGetString(GL_VERSION));
  if (version_string == nullptr) {
    LOG(WARNING) << "This system does not seem to support OpenGL.";
    return 0;
  }
  const char* const major_dot = std::strchr(version_string, '.');
  if (major_dot == nullptr || major_dot - version_string != 1) {
    LOG(WARNING) << "Unable to determine the OpenGL major version.";
    return 0;
  }
  const char* const minor_dot = std::strchr(major_dot + 1, '.');
  if (minor_dot == nullptr || minor_dot - major_dot != 2) {
    LOG(WARNING) << "Unable to determine the OpenGL minor version.";
    return 0;
  }
  return (10 * (*(major_dot - 1) - '0')) + *(minor_dot - 1) - '0';
}

// static
VisualPtr Visual::GetCurrent() {
  VisualPtr* const thread_current_visual_ptr =
      StaticVisualData::GetThreadCurrentVisualPtr();
  VisualPtr new_thread_current_visual;

  const uintptr_t current_gl_context_id = GetCurrentGlContextId();
  // Reset the current visual, if it does not match the current GL context ID.
  // This particular order of logic allows a Visual to be returned when there is
  // no current OpenGL context, if the Visual's |gl_context_id_| is 0.  This is
  // the case for MockVisual instances.
  if (*thread_current_visual_ptr) {
    if ((*thread_current_visual_ptr)->gl_context_id_ == current_gl_context_id) {
      // The Visual in |thread_current_visual_ptr| is current.
      new_thread_current_visual = *thread_current_visual_ptr;
    } else {
      // No-op.  |new_thread_current_visual| remains nullptr.
    }
  }
  if (current_gl_context_id != 0) {
    // If there is a current OpenGL context, there should exist a Visual
    // managing it.  Find it, or create one.
    new_thread_current_visual =
        StaticVisualData::FindInVisualMap(current_gl_context_id);
    if (!new_thread_current_visual) {
      // No Visual managing this context exists, so create a new wrapping one.
      new_thread_current_visual = CreateWrappingVisual();

      // CreateWrappingVisual() should have added an entry into the visual map.
      DCHECK_EQ(new_thread_current_visual.Get(),
                StaticVisualData::FindInVisualMap(current_gl_context_id).Get());
    }
  }

  // We defer assigning to |thread_current_visual_ptr| until the very end,
  // outside of any locks taken, as the assignment may require releasing the old
  // value (which may take locks while running the Visual destructor).
  *thread_current_visual_ptr = new_thread_current_visual;
  return *thread_current_visual_ptr;
}

// static
bool Visual::MakeCurrent(const VisualPtr& visual) {
  VisualPtr current_visual = GetCurrent();
  if (visual == current_visual) {
    // Return early if we are already current.
    return true;
  }

  if (current_visual) {
    // Clear the current context, if there was one.
    current_visual->ClearCurrentContextImpl();
  }

  VisualPtr* const thread_current_visual_ptr =
      StaticVisualData::GetThreadCurrentVisualPtr();
  thread_current_visual_ptr->Reset();

  if (visual) {
    if (!visual->MakeContextCurrentImpl()) {
      LOG(ERROR) << "Failed to make context current.";
      return false;
    }
    *thread_current_visual_ptr = visual;
  }
  return true;
}

// static
uintptr_t Visual::GetCurrentId() {
  const VisualPtr current = GetCurrent();
  if (!current) {
    return 0;
  }
  return current->GetId();
}

// static
VisualPtr Visual::CreateVisualInCurrentShareGroup(const VisualSpec& spec) {
  const VisualPtr current = GetCurrent();
  if (!current) {
    return VisualPtr();
  }
  return current->CreateVisualInShareGroupImpl(spec);
}

// static
void Visual::RefreshCurrentVisual() {
  const VisualPtr current = GetCurrent();
  if (!current) {
    return;
  }
  current->RefreshVisualImpl();
}

void Visual::RefreshVisualImpl() {}

void Visual::SetIds(uintptr_t id, uintptr_t share_group_id,
                    uintptr_t gl_context_id) {
  DCHECK_EQ(0, id_);
  DCHECK_EQ(0, share_group_id_);
  DCHECK_EQ(0, gl_context_id_);
  id_ = id;
  share_group_id_ = share_group_id;
  gl_context_id_ = gl_context_id;

  // Since we insert |this| into the visual map as a weak pointer, |this| must
  // already be held in a SharedPtr; i.e. SetIds() cannot be called directly
  // from a constructor.
  DCHECK_LT(0, this->GetRefCount())
      << "|this| not held in a SharedPtr "
         "(is Visual::SetIds() being called directly from a constructor?)";
  if (gl_context_id_ != 0) {
    // Only attempt to insert into the visual map if |gl_context_id_| is
    // non-zero.  (It should only be zero for the special case of the
    // MockVisual.)
    const bool inserted =
        StaticVisualData::InsertIntoVisualMap(gl_context_id_, VisualPtr(this));

    // If this insertion fails, we're basically hosed.  There's a previous
    // Visual which *thinks* it manages the GL context at |gl_context_id_|, but
    // now we're seeing that GL context again in a new Visual.  At this point
    // we're not able to tell apart the two contexts.
    //
    // This is a limitation due to the fact that we track GL contexts by pointer
    // value; if a GL context is destroyed and a new one created, there is no
    // guarantee that the new context has a distinct pointer value.  It can
    // occur if, for example:
    //
    // * Application creates a Visual.
    // * Application destroys the GL context underlying that Visual directly
    //   using platform API calls.
    // * Application creates a new Visual, and the GL implementation allocates
    //   the new context with the same context pointer value as the destroyed
    //   one.
    //
    // To avoid this sort of situation, just Don't Do Weird Stuff Directly With
    // Contexts.
    CHECK(inserted) << "multiple Visuals created for gl_context_id="
                    << gl_context_id_;
  }
}

uintptr_t Visual::GetGlContextId() const { return gl_context_id_; }

// static
uintptr_t Visual::CreateId() {
  static std::atomic<uintptr_t> next_id(1);
  return next_id++;
}

// static
uintptr_t Visual::CreateShareGroupId() {
  static std::atomic<uintptr_t> next_share_group_id(1);
  return next_share_group_id++;
}

}  // namespace portgfx
}  // namespace ion
