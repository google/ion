# This file defines a Bazel workspace for Ion. However, the Bazel support
# operates by wrapping the binaries and headers built by the existing Gyp
# rules into Bazel targets. Build Ion as documented before using the Bazel
# bindings.
workspace(name = "ion")

# Allows Bazel rules of projects depending on Ion to refer to Ion's copy
# of ABSL. Ion depends on ABSL, but it is integrated as Git submodule, not
# a Bazel http_archive.
local_repository(
    name = "com_google_absl",
    path = "third_party/absl",
)
