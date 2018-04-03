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

#ifndef ION_PORTGFX_GLCONTEXT_H_
#define ION_PORTGFX_GLCONTEXT_H_

#include <stdint.h>

#include "ion/base/sharedptr.h"
#include "ion/base/weakreferent.h"

namespace ion {
namespace portgfx {

class GlContext;
using GlContextPtr = base::SharedPtr<GlContext>;

// Specification structure for CreateGlContext().  The GlContextSpec default
// constructor provides a reasonable set of defaults.  We will add new
// fields to this struct as needed.
struct GlContextSpec {
  GlContextSpec(int width, int height)
      : backbuffer_width(width),
        backbuffer_height(height),
        depthbuffer_bit_depth(0),
        native_window(nullptr),
        debug_context_enabled(false) {}
  GlContextSpec() : GlContextSpec(1, 1) {}
  // The width of the created GlContext's default backbuffer.
  int backbuffer_width;
  // The height of the created GlContext's default backbuffer.
  int backbuffer_height;
  // The bit depth of the default depthbuffer, in bits.
  int depthbuffer_bit_depth;
  // If present, a window surface will be created for this GlContext.
  void* native_window;
  // Whether the created GlContext should use a debug context.  Only implemented
  // on GLX, at the moment.
  bool debug_context_enabled;
};

// Opaque class that sets up an offscreen OpenGL context in a
// platform-specific way to allow OpenGL calls to succeed on the current thread.
// Note that clients may do this on their own, and do not have to use a
// GlContext at all.  This class is intended for two purposes: testing GL code
// using native GL calls, and for initializing OpenGL on non-rendering threads,
// e.g. for creating and sharing contexts on worker threads.  In the case of
// worker threads, each thread must have its own GlContext.
//
// * Creation
//
// The lifetime semantics of the GlContext class closely track that of the
// OpenGL context instance it manages.  The creation functions are:
//
// * CreateGlContext() creates a GlContext de novo, which owns a new OpenGL
//   context instance.  The instance is not immediately made current.
// * GetCurrent() returns the GlContext associated with the currently current
//   OpenGL context.
//   * If there is no current OpenGL context, returns nullptr.
//   * If the current OpenGL context was created and owned by a GlContext
//     instance, returns that GlContext.
//   * If the context was created externally to a GlContext, returns the
//     (unique) GlContext instance which wraps (but does not own) the context.
//     If such a GlContext does not already exist, it is created.
//
// When the GlContext is made current on a thread, its corresponding OpenGL
// context is made current on that thread.  The converse is not necessarily
// true, however; if an OpenGL context is directly made current on a thread, the
// GlContext is not automatically made current, as most (probably all) OpenGL
// implementations provide no notification of context changes.  In this case,
// GetCurrent() should be used to resynchronize the GlContext with the context.
//
// * Lifetime
//
// These entry points return a shared pointer to the GlContext.  Any thread that
// has the GlContext as its current GlContext also holds an implicit reference
// on the GlContext; when the last client-held reference is dropped, the
// GlContext is still not destroyed until the last thread that has it current
// makes a different GlContext current.  This is analogous to OpenGL context
// implementations, which after DestroyContext() do not destroy a context until
// it is nowhere current.
//
// * Implementor's notes:
//
// In addition to the pure virtual functions defined in the GlContext interface,
// the creation functions (as explained above) are static and must be
// implemented exactly once.  Since the resulting binary is built for a given
// platform, the complexity of choosing a GlContext implementation to create at
// runtime is unwarranted, so we can hard-code the creation in a static
// function.  Thus, an implementation of GlContext should implement, as members:
//
// * virtual bool IsValid() const;
// * virtual bool MakeContextCurrentImpl();
// * virtual bool ClearCurrentContextImpl();
// * (optional) virtual void RefreshGlContextImpl();
// * virtual GlContextPtr CreateGlContextInShareGroupImpl();
//
// As static members, it should implement:
//
// * static void* GetProcAddress(const char* proc_name, bool is_core);
// * static GlContextPtr CreateGlContext();
// * static GlContextPtr CreateWrappingGlContext();
// * static uintptr_t GetCurrentGlContextId();
//
// * GlContext::CreateGlContext() is implemented by the GlContext
//   implementation, as a static function in the GlContext class.
// * GlContext::CreateGlContextInShareGroupImpl() is implemented by the
//   GlContext implementation, as a virtual member function in the
//   implementation's own derived class.  The virtual dispatch is necessary
//   since both FakeGlContext and the platform GlContext implementation may
//   co-exist in the binary.
class ION_API GlContext : public base::WeakReferent {
 public:
  // Returns true if the OpenGL initialization was successful for this
  // GlContext. Callers should not attempt to make calls to OpenGL if Ion is
  // managing the GL context, and this returns false.
  virtual bool IsValid() const = 0;

  // GetProcAddress flag that indicates that this is a core GL entry point,
  // which on some platforms is looked up differently.
  static constexpr uint32_t kProcAddressCore = 1 << 0;

  // GetProcAddress flag that indicates the entry point should be looked up
  // in a portable manner, without attempting to dlopen() the GL library itself.
  // This is highly recommended.
  static constexpr uint32_t kProcAddressPure = 1 << 1;

  // Returns a pointer to the GL entry point named |proc_name| in this
  // GlContext's OpenGL context.
  virtual void* GetProcAddress(const char* proc_name, uint32_t flags) const = 0;

  // Creates a new GlContext which owns a new GL context that is not in a share
  // group.
  static GlContextPtr CreateGlContext(
      const GlContextSpec& spec = GlContextSpec());

  // Implementation note: the following functions are common to all platforms,
  // and are implemented in glcontext.cc.

  // Returns the unique ID associated with this GlContext.  Returns 0 if
  // invalid.
  uintptr_t GetId() const;

  // Returns a unique ID for the share group to which this context belongs.
  // Returns 0 if invalid.
  uintptr_t GetShareGroupId() const;

  // Set the share group id; this is only supported on non-owned contexts,
  // because they are not created by the GlContext.
  void SetShareGroupId(uintptr_t group);

  // If this GlContext has not been stamped with GetCurrent(true), returns
  // false. Otherwise, checks if the current GL context matches this GlContext,
  // or at least lives in a share group with this GlContext's associated
  // context.
  bool DoesCurrentContextMatch() const;

  // Posts the GlContext's surface to the window on most platforms. On iOS this
  // must be done manually by the caller.
  virtual void SwapBuffers() = 0;

  // Clears the context and frees all thread-local state owned by the
  // GlContext.  If desired, a GlContext can still be re-attached to the
  // current thread, but at the cost of a small reallocation.
  //
  // For maximum efficacy, this method should be called with a GlContext already
  // current on the current thread.
  //
  // This method is useful for clients that create and destroy many
  // threads, so that memory usage doesn't grow over time.  It's also
  // useful when checking for memory leaks.
  static void CleanupThread();

  // Returns the GlContext managing the OpenGL context that is current for this
  // thread.
  // * If there is no current OpenGL context, returns nullptr.
  // * If the context was created and owned by a GlContext instance, returns
  //   that GlContext.
  // * The context was created externally to a GlContext, returns the (unique)
  //   GlContext instance which wraps (but does not own) the context.  If such a
  //   GlContext does not already exist, it is created.
  // When check_stamp is true, this function uses a slower, but fully reliable
  // method of determining the correct GlContext object, which can correctly
  // handle the GL context being changed outside of Ion to a new context with
  // the same address.
  static GlContextPtr GetCurrent(bool check_stamp = false);

  // Makes the passed GlContext the current one for this thread. The GlContext's
  // GL context is also made current. Returns true iff the GlContext was
  // successfully made current.
  static bool MakeCurrent(const GlContextPtr& context);

  // Returns the unique ID associated with the currently bound GlContext.
  // Returns 0 if no GlContext is current.
  static uintptr_t GetCurrentId();

  // Creates a new GlContext which owns a new GL context in the same share group
  // as the current GlContext.  Returns nullptr if there is no current
  // GlContext.
  static GlContextPtr CreateGlContextInCurrentShareGroup(
      const GlContextSpec& spec = GlContextSpec());

  // Refreshes the current GlContext's internal state. Implemented solely for
  // EGL to reacquire the current surface on platforms like Android that replace
  // the active surface during resize/resume operations.
  static void RefreshCurrentGlContext();

  // To be implemented by subclasses: get the ID for the currently current
  // OpenGL context.  Returns 0 if no context is current.
  static uintptr_t GetCurrentGlContextId();

 protected:
  GlContext();

  // OpenGL calls should not be made after the GlContext is destroyed.
  ~GlContext() override;

  // To be implemented by subclasses: make this GlContext's context current.
  virtual bool MakeContextCurrentImpl() = 0;

  // To be implemented by subclasses: clear the current context.
  virtual void ClearCurrentContextImpl() = 0;

  // To be implemented by subclasses: implement RefreshCurrentGlContext() on
  // platforms that require it.  The default implementation does nothing.
  virtual void RefreshGlContextImpl();

  // Frees all thread-local state owned by the GlContext.
  virtual void CleanupThreadImpl() const;

  // To be implemented by subclasses: make a new GlContext in this GlContext's
  // sharing group.
  virtual GlContextPtr CreateGlContextInShareGroupImpl(
      const GlContextSpec& spec) = 0;

  // To be implemented by subclasses: reports whether or not the underlying
  // context was created by the GlContext (owned) or whether the GlContext wraps
  // an already-existing context (non-owned).
  virtual bool IsOwned() const = 0;

  // To be implemented by subclasses: reports whether or not the current
  // underlying platform context is the one owned or wrapped by this GlContext.
  // The default implementation compares GetCurrentGlContextId() to this
  // GlContext's |gl_context_id_|; this method is overrideable for the benefit
  // of FakeGlContext, which may be considered current even when there is no
  // current underlying platform context.
  virtual bool IsCurrentGlContext() const;

  // In rare cases, the GL context may be changed outside of Ion to a new
  // context that still has the same address. To guard against this scenario,
  // this method creates a GL object in the context that identifies which
  // GlContext wraps it. GetCurrent() then checks this object to ensure that we
  // have the correct GlContext.
  virtual void MaybeCreateStamp();

  // Checks whether the stamp in the current GL context matches this GlContext.
  virtual bool CheckStamp() const;

  // To be implemented by subclasses: creates a new GlContext which wraps the
  // OpenGL context current on this thread.  The GlContext does not own this
  // context.  Returns nullptr if there is no context.
  static GlContextPtr CreateWrappingGlContext();

  // Utility functions, implemented in glcontext.cc

  // Set the IDs for ths GlContext.
  void SetIds(uintptr_t id, uintptr_t share_group_id, uintptr_t gl_context_id);

  uintptr_t GetGlContextId() const;

  // Create a new, unique, nonzero ID.
  static uintptr_t CreateId();

  // Create a new, unique, nonzero share group ID.
  static uintptr_t CreateShareGroupId();

 private:
  // GlContext unique ID.
  uintptr_t id_;

  // Share group unique ID.
  uintptr_t share_group_id_;

  // OpenGL context ID.  This is derived directly from the OpenGL
  // implementation, unlike |id_|.
  uintptr_t gl_context_id_;

  // A dummy shader object that stores the address of this GlContext in its
  // source. Used to detect GL contexts that reuse an address of a previously
  // seen context.
  uint32_t dummy_shader_id_;
};

}  // namespace portgfx
}  // namespace ion

#endif  // ION_PORTGFX_GLCONTEXT_H_
