# Ion

Ion is a portable suite of libraries and tools for building client applications,
especially graphical ones. It is small, fast, robust, and is cross-platform
across many platforms and devices, including desktops, mobile devices, browsers,
and other embedded platforms.

For more details and a fair amount of documentation see
https://google.github.io/ion.

## Why Use Ion?
* Small: < 500k binary size on mobile platforms, often much smaller
* Powerful: Tools for faster productivity when developing applications
  * Robust, portable application infrastructure aids in:
  * Object lifetime management
  * Memory allocation
  * Application start-up and static instances
  * Threading
  * Run-time setting editing
  * Automatic performance instrumentation
  * More!
* Tools for graphics:
  * Analyze graphics scenes to find performance bottleneck
  * Trace all OpenGL calls and examine their arguments
  * Use scene resources in multiple contexts, automatically
  * Run-time graphics state introspection
  * Run-time shader editing: change your shaders and immediately see the results
* Fast graphics: Minimal overhead between your application and OpenGL / ES
* Tested: Well-tested and facilitates testing your application
  * ~100% test coverage
  * Black- and white-box tested, unit tests and integration tests
  * Mock implementation of OpenGL API allows direct renderer unit tests and
    validation
  * Integrated Remote: extensible API allows changing arbitrary application
    settings on-the-fly for faster development, testing, and debugging
* Cross-platform:
  * Desktop: Linux, Mac OSX, Windows (OpenGL)
  * Handheld: Android (ARM, x86 MIPS), iOS (ARM and x86), and their 64-bit
    variants
  * Browser: Emscripten/asm.js, NaCl / pNaCl
* Cross-functional
  * Used by many teams across Google, running on billions of devices through
    multiple Google products

NOTE: This is not an official Google product.
