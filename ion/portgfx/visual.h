/**
Copyright 2016 Google Inc. All Rights Reserved.

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

#ifndef ION_PORTGFX_VISUAL_H_
#define ION_PORTGFX_VISUAL_H_

#include <memory>

#include "base/integral_types.h"
#include "base/macros.h"

namespace ion {
namespace portgfx {

// Opaque class that sets up an offscreen OpenGL context/surface/visual in a
// platform-specific way to allow OpenGL calls to succeed on the current thread.
// Note that clients may do this on their own, and do not have to use a Visual
// at all. This class is intended for two purposes: testing GL code using native
// GL calls, and for initializing OpenGL on non-rendering threads, e.g., for
// creating and sharing contexts on worker threads. The the case of worker
// threads, each thread must have its own Visual.
//
// To create a Visual that is part of a context share group, bind a valid GL
// context and construct a Visual(true). The original context can be the main
// context used for rendering (which Ion does not manage), or another Visual.
class ION_API Visual {
 public:
  // OpenGL calls should not be made after the Visual is destroyed.
  virtual ~Visual();

  // Return true if this is the current Visual on this thread.
  bool IsCurrent() const { return this == GetCurrent(); }

  // Returns true if the OpenGL initialization was successful. Callers should
  // not attempt to make calls to OpenGL if Ion is managing the GL context,
  // and this returns false.
  virtual bool IsValid() const;

  // Returns the ID associated with this Visual.
  size_t GetId() const { return id_; }

  // Returns the major and minor OpenGL version without a decimal point, e.g.
  // version 2.0 returns 20, version 4.3 returns 43.
  int GetGlVersion() const;

  // Refreshes the current visual's internal state. Implemented solely for EGL
  // to reacquire the current surface on platforms like Android that replace
  // the active surface during resize/resume operations.
  static void RefreshCurrentVisual();

  // Returns the Visual that is current for the calling thread. This has the
  // following semantics:
  // - If a Visual of type kMock is bound (only creatable by subclasses) then
  // return it.
  // - If there is no GL context bound, returns NULL.
  // - If there is a GL context bound, but no Visual bound, returns a Visual
  // that wraps it. The Visual object will be created and managed by Ion (but
  // can be explicitly destroyed via DestroyWrappingVisual(), below), and
  // associated with the GL context.
  // - If there is a GL context bound, and a Visual bound, returns the Visual
  // that actually wraps the actually bound GL context, creating one if
  // necessary (see previous item).
  static const Visual* GetCurrent();

  // Creates a new Visual that is not in a share group.  Returns NULL if there
  // is no current Visual.
  static std::unique_ptr<Visual> CreateVisual();

  // Creates a new Visual in the same share group as the current Visual.
  // Returns NULL if there is no current Visual.
  static std::unique_ptr<Visual> CreateVisualInCurrentShareGroup();

  // Makes the passed Visual the current one for this thread. The Visual's GL
  // context is also made current. Returns whether the Visual was successfully
  // made current.
  static bool MakeCurrent(const Visual* new_current);

  // If the passed Visual wraps a GL context not created by Ion, the Visual is
  // destroyed; the passed pointer should then not be used after calling this
  // function. Passing Visuals created with CreateVisual() or
  // CreateVisualInCurrentShareGroup() has no effect.
  static void DestroyWrappingVisual(const Visual* visual);

  // Returns a unique ID for the currently bound Visual. The visual's context
  // pointer is used as the id, which may be recycled, i.e. the pointer may get
  // reused when a context is destroyed and another is created.
  static size_t GetCurrentId();

 protected:
  // Constructor for subclasses that make their own contexts.
  Visual();

  // Return a newly-instantiated Visual in the same share group as this Visual.
  // If this Visual is not current, then return NULL without creating a clone.
  virtual Visual* CreateVisualInShareGroup() const;

  // Makes this Visual current for this thread and returns whether its
  // associated GL context was successfully made current. Future GL calls on
  // this thread should succeed after a return value of true.
  virtual bool MakeCurrent() const;

  // Destroy Visuals of type kNew.
  virtual void TeardownContextNew();

  // Destroy Visuals of type kShare. Should not attempt to destroy
  // parts of the current OpenGL context, surface, display, etc.
  virtual void TeardownContextShared();

  // Updates this' ID in a platform dependent way.
  virtual void UpdateId();

  // Sets the ID of this.
  void SetId(size_t id) { id_ = id; }

  // Responsible for cleaning up |visual's| resources. Calls SetCurrent(NULL) if
  // visual->IsCurrent(), and dispatches into DestroyContextNew() or
  // DestroyContextShared() for kNew and kShared visuals,
  // respectively.
  //
  // Note that if type is kNew, and the visual is current, then the
  // current OpenGL context will be cleared by ClearCurrentContext().
  static void TeardownVisual(Visual* visual);

  // Registers the passed Visual in the global list. Subclasses must call this
  // to ensure that they are returned from GetCurrent().
  static void RegisterVisual(Visual *visual);

 private:
  // The type of Visual to create.
  enum Type {
    // Uses the Visual interface but otherwise does nothing.
    kMock,
    // Creates a new GL context that is not part of a share group.
    kNew,
    // Creates a new GL context that is part of a share group with whatever GL
    // context is currently bound.
    kShare,
    // Wraps the current GL context so that it can be made current later. Will
    // not destroy the context when the Visual is destroyed.  The newly-created
    // Visual is made current (i.e. immediately calling Visual::GetCurrent()
    // will return the newly-created Visual).
    kCurrent
  };

  // If share_current_context is true then the current OpenGL context will be
  // used as a share context. Creating a share context must be done on a thread
  // with an active context. The Visual can then be used on other threads (after
  // calling MakeCurrent()) to interact with OpenGL. Note that the new context
  // is _not_ made current.
  explicit Visual(Type type);

  struct VisualInfo;
  std::unique_ptr<VisualInfo> visual_;

  // The type of this.
  Type type_;

  // The ID of this.
  size_t id_;

  friend class MockVisual;

  DISALLOW_COPY_AND_ASSIGN(Visual);
};

}  // namespace portgfx
}  // namespace ion

#endif  // ION_PORTGFX_VISUAL_H_
