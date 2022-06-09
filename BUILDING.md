# How to build Ion

## Submodules
Ion depends on several submodules, you will need to run

    git submodule init
    git submodule update

to download and sync them.

## Dependencies
Ion requires python (2.7+) to build.  It also requires gyp, which you can find
in the `third_party/gyp` directory, or install yourself using your a package
system. Depending on how you want to build Ion, you may need to install a
compiler (such as Xcode, clang, or gcc) and edit .gypi files in `dev/` to enable
the toolchain. Generally, this requires modifying the `dev/<platform>.gypi` file
to point to the right paths. For Android this is `dev/android_common.gypi`. By
default on Linux, Ion builds using clang and libc++ (but requires `libc++` and
`libc++abi` development libraries and headers).

On Ubuntu 14.04 LTS, you will need to install the following packages:

    libc++-dev
    libc++abi-dev
    clang

This will use clang 3.4, which is recent enough for Ion. If you want to use
a different version, remember to correctly set the CXX environment variable.

## Building
Build Ion and its dependencies by running `build.sh` or `build.py` from the
`ion/` directory. Note you will have to obtain ninja binaries from
https://github.com/ninja-build/ninja/releases and place them in a
`third_party/ninja/files/bin` subdirectory.

### Special Instructions for Android
If building for android, edit `ion/dev/android_common.gypi` and add the paths to
your Android NDK and SDK installations. Also edit `dev/ant.gypi` and set the
path to Apache Ant (available from https://ant.apache.org/bindownload.cgi ).
