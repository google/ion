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

#ifndef ION_PORTGFX_VISUAL_H_
#define ION_PORTGFX_VISUAL_H_

#include <stdint.h>

#include "ion/base/sharedptr.h"
#include "ion/base/weakreferent.h"

namespace ion {
namespace portgfx {

class Visual;
using VisualPtr = base::SharedPtr<Visual>;

// Specification structure for CreateVisual().  The VisualSpec default
// constructor provides a reasonable set of defaults.  We will add new
// fields to this struct as needed.
struct VisualSpec {
  VisualSpec()
      : backbuffer_width(1),
        backbuffer_height(1),
        depthbuffer_bit_depth(0),
        debug_context_enabled(false) {}
  // The width of the created Visual's default backbuffer.
  int backbuffer_width;
  // The height of the created Visual's default backbuffer.
  int backbuffer_height;
  // The bit depth of the default depthbuffer, in bits.
  int depthbuffer_bit_depth;
  // Whether the created Visual should use a debug context.  Only implemented on
  // GLX, at the moment.
  bool debug_context_enabled;
};

// Opaque class that sets up an offscreen OpenGL context/surface/visual in a
// platform-specific way to allow OpenGL calls to succeed on the current thread.
// Note that clients may do this on their own, and do not have to use a Visual
// at all.  This class is intended for two purposes: testing GL code using
// native GL calls, and for initializing OpenGL on non-rendering threads, e.g.
// for creating and sharing contexts on worker threads.  In the case of worker
// threads, each thread must have its own Visual.
//
// * Creation
//
// The lifetime semantics of the Visual class closely track that of the OpenGL
// context instance it manages.  The creation functions are:
//
// * CreateVisual() creates a Visual de novo, which owns a new OpenGL context
//   instance.  The instance is not immediately made current.
// * GetCurrent() returns the Visual associated with the currently current
//   OpenGL context.
//   * If there is no current OpenGL context, returns nullptr.
//   * If the current OpenGL context was created and owned by a Visual instance,
//     returns that Visual.
//   * If the context was created externally to a Visual, returns the (unique)
//     Visual instance which wraps (but does not own) the context.  If such a
//     Visual does not already exist, it is created.
//
// When the Visual is made current on a thread, its corresponding OpenGL context
// is made current on that thread.  The converse is not necessarily true,
// however; if an OpenGL context is directly made current on a thread, the
// Visual is not automatically made current, as most (probably all) OpenGL
// implementations provide no notification of context changes.  In this case,
// GetCurrent() should be used to resynchronize the Visual with the context.
//
// * Lifetime
//
// These entry points return a shared pointer to the Visual.  Any thread that
// has the Visual as its current Visual also holds an implicit reference on the
// Visual; when the last client-held reference is dropped, the Visual is still
// not destroyed until the last thread that has it current makes a different
// Visual current.  This is analogous to OpenGL context implementations, which
// after DestroyContext() do not destroy a context until it is nowhere current.
//
// * Implementor's notes:
//
// In addition to the pure virtual functions defined in the Visual interface,
// the creation functions (as explained above) are static and must be
// implemented exactly once.  Since the resulting binary is built for a given
// platform, the complexity of choosing a Visual implementation to create at
// runtime is unwarranted, so we can hard-code the creation in a static
// function.  Thus, an implementation of Visual should implement, as members:
//
// * virtual bool IsValid() const;
// * virtual bool MakeContextCurrentImpl();
// * virtual bool ClearCurrentContextImpl();
// * (optional) virtual void RefreshVisualImpl();
// * virtual VisualPtr CreateVisualInShareGroupImpl();
//
// As static members, it should implement:
//
// * static void* GetProcAddress(const char* proc_name, bool is_core);
// * static VisualPtr CreateVisual();
// * static VisualPtr CreateWrappingVisual();
// * static uintptr_t GetCurrentGlContextId();
//
// * Visual::CreateVisual() is implemented by the Visual implementation, as a
//   static function in the Visual class.
// * Visual::CreateVisualInShareGroupImpl() is implemented by the Visual
//   implementation, as a virtual member function in the implementation's own
//   derived class.  The virtual dispatch is necessary since both MockVisual and
//   the platform Visual implementation may co-exist in the binary.
class ION_API Visual : public base::WeakReferent {
 public:
  // Returns true if the OpenGL initialization was successful for this Visual.
  // Callers should not attempt to make calls to OpenGL if Ion is managing the
  // GL context, and this returns false.
  virtual bool IsValid() const = 0;

  // Returns a pointer to the GL entry point named |proc_name| in this Visual's
  // OpenGL context.  |is_core| is true iff this is a core GL entry point, which
  // on some platforms is looked up differently.
  virtual void* GetProcAddress(const char* proc_name, bool is_core) const = 0;

  // Creates a new Visual which owns a new GL context that is not in a share
  // group.
  static VisualPtr CreateVisual(const VisualSpec& spec = VisualSpec());

  // Implementation note: the following functions are common to all platforms,
  // and are implemented in visual.cc.

  // Returns the unique ID associated with this Visual.  Returns 0 if invalid.
  uintptr_t GetId() const;

  // Returns a unique ID for the share group to which this context belongs.
  // Returns 0 if invalid.
  uintptr_t GetShareGroupId() const;

  // Set the share group id; this is only supported on non-owned contexts,
  // because they are not created by the Visual.
  void SetShareGroup(uintptr_t group);

  // Returns the major and minor OpenGL version without a decimal point, e.g.
  // version 2.0 returns 20, version 4.3 returns 43.  This Visual must be
  // current.
  int GetGlVersion() const;

  // Clears the context and frees all thread-local state owned by the
  // Visual.  If desired, a Visual can still be re-attached to the
  // current thread, but at the cost of a small reallocation.
  //
  // This method is useful for clients that create and destroy many
  // threads, so that memory usage doesn't grow over time.  It's also
  // useful when checking for memory leaks.
  static void CleanupThread();

  // Returns the Visual managing the OpenGL context that is current for this
  // thread.
  // * If there is no current OpenGL context, returns nullptr.
  // * If the context was created and owned by a Visual instance, returns that
  //   Visual.
  // * The context was created externally to a Visual, returns the (unique)
  //   Visual instance which wraps (but does not own) the context.  If such a
  //   Visual does not already exist, it is created.
  static VisualPtr GetCurrent();

  // Makes the passed Visual the current one for this thread. The Visual's GL
  // context is also made current. Returns true iff the Visual was successfully
  // made current.
  static bool MakeCurrent(const VisualPtr& visual);

  // Returns the unique ID associated with the currently bound Visual.  Returns
  // 0 if no Visual is current.
  static uintptr_t GetCurrentId();

  // Creates a new Visual which owns a new GL context in the same share group as
  // the current Visual.  Returns nullptr if there is no current Visual.
  static VisualPtr CreateVisualInCurrentShareGroup(
      const VisualSpec& spec = VisualSpec());

  // Refreshes the current visual's internal state. Implemented solely for EGL
  // to reacquire the current surface on platforms like Android that replace
  // the active surface during resize/resume operations.
  static void RefreshCurrentVisual();

 protected:
  Visual();

  // OpenGL calls should not be made after the Visual is destroyed.
  ~Visual() override;

  // To be implemented by subclasses: make this Visual's context current.
  virtual bool MakeContextCurrentImpl() = 0;

  // To be implemented by subclasses: clear the current context.
  virtual void ClearCurrentContextImpl() = 0;

  // To be implemented by subclasses: implement RefreshCurrentVisual() on
  // platforms that require it.  The default implementation does nothing.
  virtual void RefreshVisualImpl();

  // To be implemented by subclasses: make a new Visual in this Visual's sharing
  // group.
  virtual VisualPtr CreateVisualInShareGroupImpl(const VisualSpec& spec) = 0;

  // To be implemented by subclasses: reports whether or not the underlying
  // context was created by the Visual (owned) or whether the Visual wraps
  // an already-existing context (non-owned).
  virtual bool IsOwned() const = 0;

  // To be implemented by subclasses: creates a new Visual which wraps the
  // OpenGL context current on this thread.  The Visual does not own this
  // context.  Returns nullptr if there is no context.
  static VisualPtr CreateWrappingVisual();

  // To be implemented by subclasses: get the ID for the currently current
  // OpenGL context.  Returns 0 if no context is current.
  static uintptr_t GetCurrentGlContextId();

  // Utility functions, implemented in visual.cc

  // Set the IDs for ths Visual.
  void SetIds(uintptr_t id, uintptr_t share_group_id, uintptr_t gl_context_id);

  uintptr_t GetGlContextId() const;

  // Create a new, unique, nonzero ID.
  static uintptr_t CreateId();

  // Create a new, unique, nonzero share group ID.
  static uintptr_t CreateShareGroupId();

 private:
  // Visual unique ID.
  uintptr_t id_;

  // Share group unique ID.
  uintptr_t share_group_id_;

  // OpenGL context ID.  This is derived directly from the OpenGL
  // implementation, unlike |id_|.
  uintptr_t gl_context_id_;
};

}  // namespace portgfx
}  // namespace ion

#endif  // ION_PORTGFX_VISUAL_H_
