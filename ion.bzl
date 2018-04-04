# Variable and function definitions to simplify Ion Bazel BUILD files.

# -----------------------------------------------------------------------------

def select_build_output_files_for_ion_bazel_platform(
    files = None):
    """ Standardizes select() structure for build output file sets.

    Defines one select format to choose libraries depending on a combination
    of build operating system, configuration, and cpu architecture. See the
    config_setting entries named "{os}_{config}_{arch}."

    Args:
       files: list of library file names, i.e. ["ionimage", "statetable"].
    """
    return select({
        "linux_dbg_x86": ["gyp-out/linux/dbg/lib" + lib + ".a" for lib in files],
        "linux_opt_x86": ["gyp-out/linux/opt/lib" + lib + ".a" for lib in files],
        "linux_dbg_x64": ["gyp-out/linux/dbg/lib" + lib + ".a" for lib in files],
        "linux_opt_x64": ["gyp-out/linux/opt/lib" + lib + ".a" for lib in files],
        "windows_dbg_x86": ["gyp-out/win-ninja/dbg_x86/" + lib + ".lib" for lib in files],
        "windows_opt_x86": ["gyp-out/win-ninja/opt_x86/" + lib + ".lib" for lib in files],
        "windows_dbg_x64": ["gyp-out/win-ninja/dbg_x64/" + lib + ".lib" for lib in files],
        "windows_opt_x64": ["gyp-out/win-ninja/opt_x64/" + lib + ".lib" for lib in files],
        })
