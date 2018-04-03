# Description:
#   This file defines Bazel targets for Ion. However, the Bazel support
#   operates by wrapping the binaries and headers built by the existing Gyp
#   rules into Bazel targets. Build Ion with Gyp as documented before using the
#   Bazel targets.
# MOE:begin_strip
# Note this file is moved from /geo/render/ion/opensource/bazel/ion.build to
# /BUILD in the open source release.
# MOE:end_strip

load(":ion.bzl", "select_build_output_files_for_ion_bazel_platform")

package(
    default_visibility = ["//visibility:public"],
)

config_setting(
    name = "linux_x86_64",
    values = {"cpu": "k8"},
)

config_setting(
    name = "windows_msvc",
    values = {"cpu": "x64_windows"},
)

config_setting(
    name = "dbg",
    values = {
        "compilation_mode": "dbg",
    },
)

config_setting(
    name = "fastbuild",
    values = {
        "compilation_mode": "fastbuild",
    },
)

# Defines a matrix of configurations over Bazel operating system, compile
# configuration, and CPU architecture to select Gyp build output files.
#
# See Bazel docs on Platforms and config_settings.
# https://docs.bazel.build/versions/master/platforms.html#built-in-constraints-and-platforms
# https://docs.bazel.build/versions/master/be/general.html#config_setting.constraint_values

config_setting(
    name = "linux_dbg_x86",
    constraint_values = [
        "@bazel_tools//platforms:x86_32",
        "@bazel_tools//platforms:linux",
    ],
    values = {
        "compilation_mode": "dbg",
    },
)

config_setting(
    name = "linux_opt_x86",
    constraint_values = [
        "@bazel_tools//platforms:x86_32",
        "@bazel_tools//platforms:linux",
    ],
    values = {
        "compilation_mode": "opt",
    },
)

config_setting(
    name = "linux_dbg_x64",
    constraint_values = [
        "@bazel_tools//platforms:x86_64",
        "@bazel_tools//platforms:linux",
    ],
    values = {
        "compilation_mode": "dbg",
    },
)

config_setting(
    name = "linux_opt_x64",
    constraint_values = [
        "@bazel_tools//platforms:x86_64",
        "@bazel_tools//platforms:linux",
    ],
    values = {
        "compilation_mode": "opt",
    },
)

config_setting(
    name = "windows_dbg_x86",
    constraint_values = [
        "@bazel_tools//platforms:x86_32",
        "@bazel_tools//platforms:windows",
    ],
    values = {
        "compilation_mode": "dbg",
    },
)

config_setting(
    name = "windows_opt_x86",
    constraint_values = [
        "@bazel_tools//platforms:x86_32",
        "@bazel_tools//platforms:windows",
    ],
    values = {
        "compilation_mode": "opt",
    },
)

config_setting(
    name = "windows_dbg_x64",
    constraint_values = [
        "@bazel_tools//platforms:x86_64",
        "@bazel_tools//platforms:windows",
    ],
    values = {
        "compilation_mode": "dbg",
    },
)

config_setting(
    name = "windows_opt_x64",
    constraint_values = [
        "@bazel_tools//platforms:x86_64",
        "@bazel_tools//platforms:windows",
    ],
    values = {
        "compilation_mode": "opt",
    },
)

DEFINES = select({
    ":linux_x86_64": [
        "ION_API=",
        "ION_APIENTRY=",
        "ION_PLATFORM_LINUX=1",
        "OS_LINUX=OS_LINUX",
        "ARCH_K8",
        "ION_ARCH_X86_64=1",
    ],
    ":windows_msvc": [
        "ION_API=",
        "ION_APIENTRY=APIENTRY",

        # The following are as in ion/common.gypi
        "ION_PLATFORM_WINDOWS=1",
        "NOGDI",  # Don't pollute with GDI macros in windows.h.
        "NOMINMAX",  # Don't define min/max macros in windows.h.
        "OS_WINDOWS=OS_WINDOWS",
        "PRAGMA_SUPPORTED",
        "WIN32",
        "WINVER=0x0600",
        "_CRT_SECURE_NO_DEPRECATE",
        "_WIN32",
        "_WIN32_WINNT=0x0600",
        "_WINDOWS",

        # Defaults for other headers (along with OS_WINDOWS).
        "COMPILER_MSVC",

        # Use math constants (M_PI, etc.) from the math library
        "_USE_MATH_DEFINES",

        # Allows 'Foo&&' (e.g., move constructors).
        "COMPILER_HAS_RVALUEREF",

        # Doublespeak for "don't bloat namespace with incompatible winsock
        # versions that I didn't include".
        # http://msdn.microsoft.com/en-us/library/windows/desktop/ms737629.aspx
        "WIN32_LEAN_AND_MEAN=1",
    ],
}) + select({
    ":dbg": [
        "ION_DEBUG=1",
        "ION_PRODUCTION=0",
    ],
    ":fastbuild": [
        "ION_DEBUG=1",
        "ION_PRODUCTION=0",
    ],
    "//conditions:default": [
        "ION_DEBUG=0",
        "ION_PRODUCTION=0",
    ],
})

py_binary(
    name = "zipasset_generator",
    srcs = ["ion/dev/zipasset_generator.py"],
    visibility = ["//visibility:public"],
)

ION_EXTERNAL_LIB_NAMES = [
    "ionb64",
    "ionharfbuzz",
    "ionlodepnglib",
    "ionopenctm",
    "ionmongoose",
    "ionstblib",
    "ionzlib",
    "ionimagecompression",
]

cc_library(
    name = "ion_external_libs",
    srcs = select_build_output_files_for_ion_bazel_platform(ION_EXTERNAL_LIB_NAMES),
    includes = ["third_party/zlib/src"],
)

cc_library(
    name = "ion_jsoncpp",
    srcs = select_build_output_files_for_ion_bazel_platform(["ionjsoncpp"]),
    hdrs = glob(["third_party/jsoncpp/include/json/*.h"]),
    strip_include_prefix = "third_party/jsoncpp/include/",
)

cc_library(
    name = "base",
    defines = DEFINES,
    textual_hdrs = glob(["base/*.h"]),
)

cc_library(
    name = "util",
    defines = DEFINES,
    textual_hdrs = glob(["util/*.h"]),
)

cc_library(
    name = "ion_external_gtest",
    defines = DEFINES,
    textual_hdrs = glob(["ion/external/gtest/*.h"]),
)

cc_library(
    name = "ion_third_party_google_absl_base",
    hdrs = glob(["third_party/google/absl/base/*.h"]),
    defines = DEFINES,
    # Allows #include "absl/base/integral_types.h" path.
    strip_include_prefix = "third_party/google",
)

cc_library(
    name = "ion_port_override_base_stripped",
    hdrs = glob(include = [
        "ion/port/override/base/*.h",
        "ion/port/override/absl/base/*.h",
    ]),
    defines = DEFINES,
    # Prioritizes the override headers above the actual libraries.
    includes = ["ion/port/override/"],
    # Forms a virtual include location to overlay these files.
    strip_include_prefix = "ion/port/override",
    deps = [
        ":ion_third_party_google_absl_base",
        ":util",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_library(
    name = "ion_port_override_base",
    # MOE:begin_strip
    # Allows the base/port.h to use a full path (relative to the Ion workspace)
    # to guarantee retrieving the override absl/base/port.h header.
    # MOE:end_STRIP
    hdrs = glob(["ion/port/override/absl/base/*.h"]),
    defines = DEFINES,
    deps = [
        ":ion_port_override_base_stripped",
    ],
)

cc_library(
    name = "ion_base_stlalloc",
    hdrs = glob(["ion/base/stlalloc/*.h"]),
    defines = DEFINES,
    deps = [
        ":ion_external_gtest",
    ],
)

cc_library(
    name = "ion_third_party_GL",
    hdrs = glob([
        "third_party/GL/gl/include/**/*.h",
    ]),
    defines = DEFINES,
)

cc_library(
    name = "ionbase",
    srcs = select_build_output_files_for_ion_bazel_platform(["ionbase"]),
    hdrs = glob(["ion/base/*.h"]),
    defines = DEFINES,
    deps = [
        ":base",
        ":ion_base_stlalloc",
        ":ion_external_libs",
        ":ionlogging",
        ":ionport",
    ],
)

cc_library(
    name = "ionlogging",
    srcs = select_build_output_files_for_ion_bazel_platform(["ionlogging"]),
    hdrs = glob(["ion/base/logging.h"]),
    defines = DEFINES,
)

cc_library(
    name = "iongfx",
    srcs = select_build_output_files_for_ion_bazel_platform(["iongfx"]),
    hdrs = glob(
        ["ion/gfx/*.h"],
        exclude = [
            "ion/gfx/tracinghelper.h",
        ],
    ),
    defines = DEFINES,
    textual_hdrs = glob(["ion/gfx/*.inc"]),
    deps = [
        ":base",
        ":graphicsmanager",
        ":ion_port_override_base",
        ":ion_third_party_GL",
        ":ionportgfx",
    ],
)

cc_library(
    name = "iongfxutils",
    srcs = select_build_output_files_for_ion_bazel_platform(["iongfxutils"]),
    hdrs = glob(["ion/gfxutils/*.h"]),
    defines = DEFINES,
    deps = [
        ":ion_third_party_GL",
        ":ionbase",
        ":iongfx",
        ":ionport",
    ],
)

cc_library(
    name = "ionmath",
    srcs = select_build_output_files_for_ion_bazel_platform(["ionmath"]),
    hdrs = glob(["ion/math/*.h"]),
    defines = DEFINES,
    deps = [
        ":base",
        ":ionport",
    ],
)

cc_library(
    name = "ionportgfx",
    srcs = select_build_output_files_for_ion_bazel_platform(["ionportgfx"]),
    hdrs = glob(["ion/portgfx/*.h"]),
    defines = DEFINES,
    deps = [
        ":ionport",
    ],
)

cc_library(
    name = "ionport",
    srcs = select_build_output_files_for_ion_bazel_platform(["ionport"]),
    hdrs = glob(["ion/port/*.h"]),
    defines = DEFINES,
    linkopts = select({
        "linux_x86_64": ["-ldl"],
        "//conditions:default": [],
    }),
    deps = [
        ":ion_port_override_base",
    ],
)

cc_library(
    name = "ionimage",
    srcs = select_build_output_files_for_ion_bazel_platform(["ionimage"]),
    hdrs = glob(["ion/image/*.h"]),
    defines = DEFINES,
    deps = [
        ":ion_external_libs",
        ":ion_port_override_base",
    ],
)

cc_library(
    name = "ionprofile",
    srcs = select_build_output_files_for_ion_bazel_platform(["ionprofile"]),
    hdrs = glob(["ion/profile/*.h"]),
    defines = DEFINES,
)

cc_library(
    name = "statetable",
    srcs = select_build_output_files_for_ion_bazel_platform(["statetable"]),
    hdrs = glob(["ion/gfx/statetable.h"]),
    defines = DEFINES,
)

cc_library(
    name = "graphicsmanager",
    srcs = select_build_output_files_for_ion_bazel_platform(["graphicsmanager"]),
    defines = DEFINES,
    deps = [
        ":tracinghelper",
    ],
)

cc_library(
    name = "tracinghelper",
    srcs = select_build_output_files_for_ion_bazel_platform(["tracinghelper"]),
    hdrs = glob(["ion/gfx/tracinghelper.h"]),
    defines = DEFINES,
    deps = [
        ":ionportgfx",
    ],
)

cc_library(
    name = "ionremote",
    srcs = select_build_output_files_for_ion_bazel_platform(["ionremote"]),
    hdrs = glob(["ion/remote/*.h"]),
    defines = DEFINES,
)
