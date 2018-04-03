#
# Copyright 2017 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
"""A build script for Ion and potentially anything that depends on it.

This script wraps gyp (the build file generator) and all the available build
tools and deals with the platform-specific differences on how the tools are
invoked. It should serve as a one stop shop for all your building and testing
needs.

To add a new build type:
  1. create a subclass of TargetBuilder.
  2. give it a TARGET_OS attribute and optional TARGET_FLAVOR.
  3. override any relevant methods.
  4. decorate it with @RegisterBuilder.
"""

import argparse
import collections
import getpass
import hashlib
import itertools
import json
import multiprocessing
import multiprocessing.dummy
import os
import platform
import shutil
import subprocess
import sys

# The absolute path to the build root.
MAIN_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

# The absolute path to the root directory.
ROOT_DIR = MAIN_DIR

# The absolute path to the ion codebase.
ION_DIR = os.path.join(MAIN_DIR, 'ion')

# Paths to ninja binaries for various platforms.
NINJA_DIR = os.path.join(MAIN_DIR, 'third_party/ninja/files/bin')
NINJA_LINUX_BINARY_PATH = os.path.join(NINJA_DIR, 'ninja-linux64')
NINJA_MAC_BINARY_PATH = os.path.join(NINJA_DIR, 'ninja-mac')
NINJA_WINDOWS_BINARY_PATH = os.path.join(NINJA_DIR, 'ninja.exe')

# The path to LLVM, used in the asmjs builder.  Currently works on Linux only.
LLVM_PATH = os.path.abspath(os.path.join(
    ROOT_DIR, 'third_party/emscripten/llvm-bin'))

# The path to nodejs, used in the asmjs builder.
NODEJS_BINARY_PATH = os.path.abspath(os.path.join(
    ROOT_DIR, 'third_party/nodejs/bin/node'))

# Path to android toolchain, used to help find certain toolchain binaries for
# steps that are not handled by gyp.
ANDROID_NDK_DIR = os.path.join(
    MAIN_DIR,
    'third_party/android_ndk/android-ndk-r10/4.9/'
    '{arch}/{host}')

GYP_PATH = os.path.abspath(os.path.join(ROOT_DIR, 'third_party/gyp'))

# This is the path to os.gypi, which holds our configurations data. The path
# is relative to ION_DIR. This file should be force-included in order to build
# with the various Ion toolchains. This file should NOT be included by other
# projects, however.
CONFIGURATION_GYPI = 'dev/os.gypi'

# This is the path to common_variables.gypi, which holds our variables (flags).
# The path is relative to whatever gypfile is being built.
COMMON_VARIABLES = 'common_variables.gypi'

NACL_SDK_DIR = os.path.abspath(os.path.join(
    ROOT_DIR, 'third_party/native_client_sdk/pepper_55'))

# Enable importing gyp directly.
sys.path.insert(1, os.path.join(GYP_PATH, 'pylib'))
# pylint: disable=g-import-not-at-top,g-bad-import-order
import gyp

sys.path.insert(1, os.path.join(ROOT_DIR, 'third_party/py'))
try:
  # pylint: disable=g-import-not-at-top,g-bad-import-order
  import colorama
except ImportError:
  # Remove this as soon as we know p4_depot_paths have been updated everywhere.

  class FakeColorama(object):
    """Fake colorama module."""

    class FakeAnsiCodes(object):
      """Fake colorama.AnsiCodes module."""
      RED = ''
      YELLOW = ''
      GREEN = ''
      RESET = ''
      RESET_ALL = ''
      DIM = ''

    # pylint: disable=invalid-name
    Fore = FakeAnsiCodes()

    # pylint: disable=invalid-name
    Style = FakeAnsiCodes()

    def init(self, **kwargs):
      pass

  colorama = FakeColorama()


# In order to use pymod_do_main, we need to add the path to the module(s) to
# sys.path so that python can find them. Right now, they exist in ./dev.
sys.path.insert(
    0, os.path.join(os.path.abspath(os.path.dirname(__file__)), 'dev'))

# The path to the directory where built stuff goes.
GYP_OUT_DIR = os.path.abspath(os.path.join(ROOT_DIR, 'gyp-out'))

# The path to the directory where gyp project files will be output.
GYP_PROJECTS_DIR = os.path.abspath(
    os.path.join(ROOT_DIR, 'gyp-projects/build'))


def _AsyncSubprocess(stdout_lock, test_target, command_args, is_verbose):
  """Run the command line using subprocess, queueing the output.

  This exists as a top-level function to be compliant with how
  multiprocessing.apply_async works (everything must be pickleable, which
  instance methods are not).

  Standard output of the command is buffered and printed after the command runs
  in case of failure, or if is_verbose.

  Args:
    stdout_lock: multiprocessing.Lock() object, to guard against standard output
                 corruption.
    test_target: The name of the test target.
    command_args: the full command line to run.
    is_verbose: print subprocess output even on success.

  Returns:
    Tuple: (return code, standard output text).
  """
  try:
    proc = subprocess.Popen(command_args, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    stdout, _ = proc.communicate()
    retcode = proc.wait()
    stdout_lock.acquire()
    if retcode or is_verbose:
      if retcode:
        sys.stdout.write('{red}FAILED: '.format(red=colorama.Fore.RED))
      _PrintTestHeader(test_target)
      print >>sys.stdout, '{reset}{stdout}'.format(
          reset=colorama.Fore.RESET, stdout=stdout)
    else:
      print '{green}PASSED: {reset}{t}'.format(
          t=test_target, green=colorama.Fore.GREEN, reset=colorama.Fore.RESET)
    stdout_lock.release()
  except KeyboardInterrupt:
    return -1
  return retcode


class _FakeAsyncResult(object):
  """This is a fake multiprocessing.AsyncResult class.

  This is for use in testing where the platform/tests do not support
  parallel invocation for whatever reason. To make such cases fit within the
  multiprocessing.Pool model we are using (see below), we wrap their direct exit
  codes in this class so they can be used without altering the pattern of the
  Pool algorithm.
  """

  def __init__(self, retcode):
    self.retcode = retcode

  # pylint: disable=invalid-name
  def get(self):
    return self.retcode


def _PrintTestHeader(test_target_name):
  """Print a unified test header to identify the start of a test.

  Args:
    test_target_name: The name of the test.
  """
  print '=' * 30, test_target_name, '=' * 30


# pylint: disable=invalid-name
def _FindNearestFiles(anchor, filename_to_find, root_dir=ROOT_DIR):
  """Find a file with the given filename *nearest* the anchor file.

  This file will return the closest filename_to_find to the base filename, going
  up the directory tree. For example:

    anchor: ion/base/base.gyp
    filename_to_find: common_variables.gypi

  Will look for files named:

    $root_dir/ion/base/common_variables.gypi
    $root_dir/ion/common_variables.gypi
    $root_dir/geo/render/common_variables.gypi
    $root_dir/geo/common_variables.gypi
    $root_dir/common_variables.gypi

  in that order.

  Args:
    anchor: The path to the anchor file being considered. This is used as an
        anchor for searching for the filename_to_find.  It must be relative to
        root_dir.
    filename_to_find: The filename to find (can have a leading directory
        component).
    root_dir: Directory to search up to. Note that anchor must be relative to
        this.

  Returns:
    The path (including root_dir) to the filename_to_find that's closest to
    anchor, or None if not found.
  """

  potential_dir = os.path.dirname(os.path.normpath(anchor)).split(os.sep)
  while potential_dir:
    potential_file = os.path.join(root_dir,
                                  os.path.join(*potential_dir),
                                  filename_to_find)
    if os.path.isfile(potential_file):
      return potential_file
    else:
      potential_dir.pop()

  # No file found.
  return None


def _SmartDeleteDirectory(path):
  """Delete a directory, or a symlink to a directory.

  Deleting a symlink to a directory requires extra effort than usual.

  Args:
    path: The directory to delete, or a symlink to a directory to delete. In the
          latter case, the target of the symlink AND the symlink will be
          deleted.
  """

  if os.path.isdir(path) and not os.path.islink(path):
    shutil.rmtree(path)
  elif os.path.islink(path):
    target = os.readlink(path)

    shutil.rmtree(target)
    os.unlink(path)


class Error(Exception):
  """Base class for all script-specific errors."""
  pass


class InvalidTargetOSError(Error):
  """Raised when a target OS is invalid for the current host OS."""
  pass


class WorkingDirectory(object):
  """Context manager for changing the current working directory."""

  def __init__(self, new_dir):
    self.new_dir = new_dir

  def __enter__(self):
    self.old_dir = os.getcwd()
    os.chdir(self.new_dir)

  def __exit__(self, etype, value, traceback):
    os.chdir(self.old_dir)


class EnvironmentVariables(object):
  """Context manager for changing the current environment variables."""

  def __init__(self, environ):
    self.new_environ = environ

  def __enter__(self):
    self.old_environ = os.environ
    os.environ = self.new_environ

  def __exit__(self, etype, value, traceback):
    os.environ = self.old_environ


class TargetBuilder(object):
  """Class that encapsulates the details of building a project.

  This base class assumes ninja as the build tool. Subclasses can change this to
  use other build tools.
  """
  # The name of the OS for which this builder builds.  Subclasses must override
  # this value.  This is passed to gyp as --OS.
  TARGET_OS = None

  # An optional flavor specialization.  If set in a subclass, the builder can be
  # used by passing -o <os>-<flavor>.  Additionally, the flavor will be passed
  # to gyp in a gyp variable called "flavor".
  TARGET_FLAVOR = ''

  # The list of host OSes from which this builder can be built.  Subclasses must
  # override this value.  Should be a subset of ['linux', 'mac', 'win'].
  POSSIBLE_HOST_OS = []

  # The name of the gyp generator that should be used to generate project files
  # for this builder.  None means gyp will use the default generator for the
  # host OS.
  GYP_GENERATOR = None

  # The subdirectory under gyp-out/ in which to build everything this
  # TargetBuilder builds. This should be distinct to the subclass's target OS,
  # flavor, and generator values.
  #
  # This is defined as a separate property, because generating the value based
  # on the subclass's OS, flavor, and generator values is not always
  # straight forward.
  GYP_OUT_SUBDIR = None

  # The number of processes in the multiprocessing.Pool that will be used to
  # execute tests, if possible for the given platform.
  TEST_RUNNER_POOL_SIZE = multiprocessing.cpu_count()

  @classmethod
  def NinjaForHost(cls, host_os):
    """Return the path to the correct ninja binary.

    Args:
      host_os: one of the valid host OSes.

    Returns:
      Full path to ninja binary.
    """
    mapping = {
        'linux': NINJA_LINUX_BINARY_PATH,
        'mac': NINJA_MAC_BINARY_PATH,
        'win': NINJA_WINDOWS_BINARY_PATH,
    }
    return mapping[host_os]

  def __init__(self, state):
    self.cmdflags = state.GetCommandLineOptions()
    self.variables = state.GetActiveBuildFlags()
    self.variables.update(state.GetAdditionalGypVariables())
    self.state = state
    self.host_os = state.host_os
    # The dummy module uses threads behind the scenes instead of processes. This
    # results in more stable runs, as there are some signal handling issues
    # (KeyboardInterrupt) that come into play with processes.
    self.pool = multiprocessing.dummy.Pool(processes=self.TEST_RUNNER_POOL_SIZE)

  def GypGenerator(self):
    """Returns the generator this builder will pass to gyp."""
    return self.GYP_GENERATOR

  def GypFilePath(self, filename):
    """Returns the path to the gypfile this builder will pass to gyp.

    This will usually just return filename without modifications, except for
    ninja generators, where an absolute path will be returned instead to work
    around a ninja bug.

    Most subclasses will not need to override this.

    Args:
      filename: The path to the gypfile the user asked us to pass to gyp, as
          a relative path from ROOT_DIR.

    Returns:
      Path (relative or absolute) to the gypfile gyp should run on.
    """
    if self.GYP_GENERATOR == 'ninja':
      # Without abspath, ninja throws "duplicate rule" errors.

      # absolute path here may cause --generator-output not to work.
      return os.path.normpath(os.path.join(ROOT_DIR, filename))
    return filename

  def GypVariables(self):
    """Returns the variables that should be explicitly passed to gyp.

    By default, this just returns the variables passed to __init__.  Most
    subclasses will not need to override this.

    Returns:
      dict of gyp variables and values to pass to gyp.
    """
    return self.variables

  def GypDefines(self):
    """Returns a dictionary of gyp defines this builder will pass to gyp.

    The defines are generated from the class attributes, current host OS, and
    other various state.  All of the defines specified here are necessary for
    gyp to work correctly.  If you override this in subclasses, be sure to call
    the superclass method and add additional keys to what it returns rather than
    building a new dict from scratch.

    Returns:
      dict of defines to pass to gyp.
    """
    gyp_defines = {}
    gyp_defines['host_os'] = self.host_os
    gyp_defines['python'] = sys.executable
    gyp_defines['build_py'] = os.path.abspath(__file__)
    gyp_defines['OS'] = self.TARGET_OS
    gyp_defines['flavor'] = self.TARGET_FLAVOR
    gyp_defines['gyp_out_os_dir'] = self.BuildOutputRootDir()

    # Chromium-based gyp files contain conditionals based of target_arch, so
    # this variable must at least exist.
    gyp_defines['target_arch'] = ''
    return gyp_defines

  def GypArgs(self):
    """Returns a dictionary of extra arguments this builder will pass to gyp.

    By default, this specifies -Goutput_dir and --depth, both of which are
    necessary for gyp to work correctly.  If you override this in subclasses, be
    sure to call the superclass method and add additional args rather than
    building a new dictionary from scratch.

    If the value for a key is a list containing multiple elements, the argument
    will be passed multiple times to gyp.  If it is a string or a single-element
    list, it will be passed only once.  (And if it's an empty list, it won't be
    passed at all.)

    The dictionary returned by this method is actually a collections.defaultdict
    that uses an empty list as the default value for any key not otherwise
    specified.  This means that it is okay for subclasses to blindly append
    values to a given key, but note that it is still possible to clobber
    existing keys by using = instead of +=, so only do that if you mean to.

    Returns:
      A collections.defaultdict of extra arguments to pass to gyp.
    """
    generator_flags = self.state.GetAdditionalGypGeneratorFlags()
    generator_flags.append('output_dir={o}'.format(o=self.BuildOutputRootDir()))
    return collections.defaultdict(list, {
        '-G': generator_flags,
        '--depth': '{m}'.format(m=ROOT_DIR),
        '--check': None,
        '--suffix': '_{p}'.format(p=self.TARGET_OS),
    })

  def GypCWD(self):
    """Returns the working directory from which gyp should be run.

    By default, this returns ROOT_DIR, but you can override that in a
    subclass if you need to run gyp from some other directory.

    Returns:
      An absolute path to the working directory from which gyp should be run.
    """
    return ROOT_DIR

  def GypEnv(self):
    """Returns the environment variables to use when running gyp.

    By default, this just returns a copy of os.environ.

    Returns:
      A dictionary of environment variables.
    """
    env = os.environ.copy()
    env['GYP_CROSSCOMPILE'] = '1'
    return env

  def CanBuildOnHost(self):
    """Returns whether this builder can build on the current host OS."""
    return self.host_os in self.POSSIBLE_HOST_OS

  def BuildCWD(self):
    """Returns the working directory from which the build should be run.

    By default, this returns ROOT_DIR, but you can override that in a
    subclass if the build system needs to work from some other directory.

    Returns:
      An absolute path to the working directory from which the build tool should
      be run.
    """
    return ROOT_DIR

  def BuildOutputRootDir(self):
    """Returns the path to where build output should go.

    By default, this returns ROOT_DIR/gyp-out/<GYP_OUT_SUBDIR>. See
    subclasses for what GYP_OUT_SUBDIR looks like for different target
    platforms/generators/flavors.

    Returns:
      An absolute path to the build output root directory.
    """
    return os.path.join(GYP_OUT_DIR, self.GYP_OUT_SUBDIR)

  def BuildOutputDir(self, configuration):
    """Returns the path to where build output should go for this configuration.

    By default, this returns a directory named <configuration> inside the
    directory pointed to by BuildOutputRootDir(). Most subclasses shouldn't need
    to override this.

    Args:
      configuration: The configuration to get the directory for.

    Returns:
      An absolute path to the build output directory for the configuration.
    """
    return os.path.join(self.BuildOutputRootDir(), configuration)

  def BuildEnv(self, unused_configuration=None):
    """Returns the environment variables to use when running the build tool.

    By default, this just returns a copy of os.environ.

    Args:
      unused_configuration: The configuration being built.  This is unused by
          the base class (hence the name), but subclasses may need it.

    Returns:
      A dictionary of environment variables.
    """
    return os.environ.copy()

  def BuildBinaryPath(self):
    """Returns the path to the build tool for this builder.

    By default, this returns a path for the ninja executable on this host.
    Subclasses using generators other than ninja will need to override this.

    Returns:
      An absolute path to the build tool for this builder.
    """
    # Default to ninja binary. Subclasses can override.

    # implementations in TargetBuilder with NotImplementedErrors.
    return self.NinjaForHost(self.host_os)

  def BuildArgs(self, configuration, unused_filename):
    """Return the arguments this builder should pass to the build tool.

    By default, this specifies arguments relevant to ninja.  Subclasses using
    generators other than ninja will need to override this.

    Args:
      configuration: The configuration we're interested in building.
      unused_filename: the .gyp file we ran gyp with.

    Returns:
      A list of arguments to pass to the build tool.
    """

    # Default to ninja build arguments. Subclasses can override.
    build_args = []
    if self.cmdflags.threads is not None:
      build_args += ['-j', str(self.cmdflags.threads)]
    else:
      build_args += ['-j', str(multiprocessing.cpu_count())]

    build_args += ['-C', self.BuildOutputDir(configuration)]
    if self.cmdflags.keep_going:
      build_args += ['-k', '0']  # 0 means continue until INT_MAX failures.
    if self.cmdflags.verbose:
      build_args += ['-v']
    return build_args

  def CleanBuildOutputDirectory(self):
    """Delete the build output directory.

    This does not discriminate between configurations. Everything under the
    gyp-out/$OS directory is deleted.
    """
    PrintStatus('Removing '+ self.BuildOutputRootDir())
    if os.path.isdir(self.BuildOutputRootDir()):
      _SmartDeleteDirectory(self.BuildOutputRootDir())

  def CleanGeneratorDirectory(self):
    """Delete the generator files directory (makefiles, xcode projects, etc).

    This does not discriminate between different generators! If there are
    android makefiles AND xcodeproject files, they are all deleted.
    """
    # Note that the TargetBuilder base class is ninja-based, which stores its
    # generated files under BuildOutputRootDir(), so this method does nothing
    # here. Subclasses (that don't use ninja) should override and actually
    # delete things.
    pass

  def PathToBuiltTest(self, configuration, target):
    """Return the absolute path to the built target.

    By default, assumes that test binaries are placed in a "tests" directory
    beneath the build output directory.  Subclasses can override to provide
    paths to bundles, etc.

    Args:
      configuration: the build configuration whose built test we want to run.
      target: path (relative or absolute) to a .gyp file, along with the target,
              e.g. 'port/tests/port_test.gyp:port_test'.

    Returns:
      Absolute path to the target (or bundle) on disk.
    """
    target_name = target.split(':')[-1]
    path = os.path.join(
        self.BuildOutputDir(configuration), 'tests', target_name)

    return path

  def _ConstructGypFlags(self):
    """Constructs a dictionary of command-line flags to pass to gyp.

    The builder's implementations of GypArgs, GypGenerator, and GypDefines are
    used to construct these flags.  Subclasses should override those methods
    instead of this one.

    Returns:
      A dictionary of command-line flags.  The format of this dictionary is as
      described in GypArgs().
    """
    gyp_args = self.GypArgs()

    gyp_generator = self.GypGenerator()

    for k, v in self.GypDefines().iteritems():
      gyp_args['-D'].append('{k}={v}'.format(k=k, v=v))

    for k, v in self.GypVariables().iteritems():
      gyp_args['-D'].append('{k}={v}'.format(k=k, v=v))


    # separate builder for this.
    if self.cmdflags.gypd:
      gyp_generator = 'gypd'

    if gyp_generator:
      gyp_args['-f'] = gyp_generator

    # For testability, we really want our -D's in a well-known order that won't
    # change unpredictably, so let's sort the list.
    gyp_args['-D'].sort()

    return gyp_args

  @classmethod
  def _ConstructGypArgs(cls, flags, filename):
    """Constructs the list of command-line arguments to pass to gyp.

    Flags with multiple values are unpacked into multiple flags.  Short flags
    are rendered as two adjacent tokens in the list.  Long flags are rendered
    as a single token, with flag and value separated by an equals sign.

    Args:
      flags: The dictionary of command-line flags to be passed to gyp.  The
          format of this dictionary is as described in GypArgs().
      filename: The full relative path to the gypfile that should be passed to
          gyp, as provided by the GypFilePath() method.

    Returns:
      A list of command-line arguments to gyp.
    """
    def FormatArgAndValue(arg, value):
      if arg.startswith('--'):
        return ['{arg}={value}'.format(arg=arg, value=value)]
      else:
        return [arg, value]

    command_line = []
    for k, v in flags.iteritems():
      if isinstance(v, basestring):
        # Just add the key and value.
        command_line += FormatArgAndValue(k, v)
      elif v is None:
        # Argument takes no value.
        command_line.append(k)
      else:
        # Add the key multiple times, once with each value, as a flat list.
        command_line += list(
            itertools.chain.from_iterable([FormatArgAndValue(k, x) for x in v]))

    # Newer versions of gyp complain if there are multiple source files with the
    # same filename in the same gyp files (even if they are in different targets
    # or directories). Unfortunately Ion and Ion-dependent projects have many
    # such cases, so we currently need to ignore this warning.
    command_line.append('--no-duplicate-basename-check')

    command_line.append(filename)

    return command_line

  def RunGyp(self, filename):
    """Runs gyp with appropriate settings.

    Args:
      filename: The name of the gyp file that will be passed to gyp.

    Raises:
      InvalidTargetOSError: Can't build target OS on this host.

    Returns:
      Return code of gyp process.
    """
    if not self.CanBuildOnHost():
      raise InvalidTargetOSError(
          'Cannot build for target OS "{0}" on host OS "{1}"'.format(
              self.TARGET_OS, self.host_os))

    flags = self._ConstructGypFlags()
    # Since we know what gyp target we're building, force include the
    # configuration file, if it exists (some projects can use this
    # infrastructure without having a dev/os.gypi).
    configuration_gypi = _FindNearestFiles(filename, CONFIGURATION_GYPI)
    if configuration_gypi:
      flags['-I'] = [configuration_gypi]

    gyp_args = self._ConstructGypArgs(flags,
                                      self.GypFilePath(filename))
    gyp_env = self.GypEnv()
    gyp_cwd = self.GypCWD()

    print 'gyp {0}'.format(' '.join(gyp_args))

    with WorkingDirectory(gyp_cwd):
      with EnvironmentVariables(gyp_env):
        ret = gyp.main(gyp_args)

    return ret

  def RunBuild(self, configuration, filename):
    """Runs the OS specific build tool, e.g. xcodebuild, ninja, etc.

    Determines the following variables based on host_os/target_os:
       build_binary_path, build_args, cwd, env
    and then invokes the build tool.

    Args:
      configuration: The gyp configuration to build.
      filename:      gyp filename whose output is to be built.

    Raises:
      InvalidTargetOSError: Can't build target OS on this host.

    Returns:
      Return code of invoked build tool.
    """
    if not self.CanBuildOnHost():
      raise InvalidTargetOSError('Cannot build target on this host')

    build_env = self.BuildEnv(configuration)
    build_cwd = self.BuildCWD()
    build_args = self.BuildArgs(configuration, filename)
    build_binary_path = self.BuildBinaryPath()

    # Invoke build tool
    call_list = [build_binary_path] + build_args
    print ' '.join(call_list)
    return subprocess.call(call_list, cwd=build_cwd, env=build_env)

  def RunTest(self, configuration, test_target, test_args):
    """Given a configuration, test name, and test arguments, run the test.

    This can be overridden and customized to run tests in emulators, simulators,
    etc.

    The base class assumes the test is runnable on the host OS, so it just
    spawns the test itself.

    Args:
      configuration: the build configuration whose built test we want to run.
      test_target: path (relative or absolute) to a .gyp file, along with the
          target, e.g. 'port/tests/port_test.gyp:port_test'.
      test_args: list of command line arguments for test, or None.

    Returns:
      The multiprocessing.AsyncResult object of the process. Use .get() on this
      to get the return code of the binary.
    """
    command_args = [self.PathToBuiltTest(configuration, test_target)]
    if test_args:
      command_args.extend(test_args)

    async_result = self.pool.apply_async(
        _AsyncSubprocess, args=(self.stdout_lock, test_target, command_args,
                                self.cmdflags.verbose))
    return async_result

  def GlobalTestSetup(self):
    """Conduct any setup necessary for running tests.

    This is called once before ALL of the RunTest() methods are called.
    """
    pass

  def RunTests(self, configuration, filename, test_args=None,
               stop_on_failure=False):
    """Run the tests that we built (if any).

    This routine examines the .gyp file that the TargetBuilder has just
    generated/built for targets that look like unittests (ending in "_test").

    The actual test binaries will have to be found on disk, depending on the
    build method.

    This method tries to use a multiprocessing.Pool() to run the tests in
    parallel (unless stop_on_failure is given, in which case everything is run
    serially). It prevents interleved stdout by supplying a Lock() object to the
    test processes.

    Args:
      configuration: the build configuration whose built test we want to run.
      filename: path (relative or absolute) to a .gyp file, along with the
          target, e.g. 'port/tests/port_test.gyp:port_test'.
      test_args: optional list of command line arguments for test.
      stop_on_failure: if True, causes this routine to return immediately after
          the first failure.

    Returns:
      If stop_on_failure: the return code of the first failing tests. Otherwise,
      Bitwise-OR of all return codes of tests.
    """

    dump_dependency_json = DumpDependencyBuilder(
        self.state, target_os=self.TARGET_OS, target_flavor=self.TARGET_FLAVOR)
    dependency_dump = dump_dependency_json.GetDependencyJson(filename)

    targets = dependency_dump.keys()

    # Remove the trailing '#target' from all targets.
    targets = [t.split('#target')[0] for t in targets]

    # Remove targets that don't end in '_test', leaving only the unittests.
    test_targets = set([t for t in targets if t.endswith('_test')])

    # Conduct any one-time global test setup before running the tests.
    self.GlobalTestSetup()

    PrintStatus('Running {0} test targets.'.format(len(test_targets)))

    self.stdout_lock = multiprocessing.dummy.Lock()

    results = []
    for test_target in test_targets:
      result = self.RunTest(configuration, test_target, test_args)

      # result can be an AsyncResult object, or an exit code (integer) if the
      # RunTest method couldn't be parallelized. If the latter, wrap it in a
      # _FakeAsyncResult() so it can be handled seemlessly below.
      if isinstance(result, int):
        result = _FakeAsyncResult(result)

      results.append((test_target, result))

      if stop_on_failure:
        # We can't be too parallel here, since the user requested
        # stop_on_failure. Instead, force synchronous behavior by waiting on
        # the result object.
        retcode = result.get()
        if retcode:
          # Do not change the string below! It is used in test result filtering
          # by pulse.
          print 'TEST RETURNED NON-ZERO:', test_target
          return retcode

    # Wait for everything to finish.
    self.pool.close()
    self.pool.join()

    retcode_cumulative = 0
    for test_target, result in results:
      # This will raise an exception if there was an exception in the async
      # process, which is the behavior what we want.
      retcode = result.get()
      if retcode:
        # Do not change the string below! It is used in test result filtering
        # by pulse.
        print 'TEST RETURNED NON-ZERO:', test_target
      retcode_cumulative |= retcode

    return retcode_cumulative


# A registry mapping environments to builders. Keys are tuples:
#   (<target_os>, <gyp_generator>)
# Values are the builder classes.
#
# The @RegisterBuilder decorator automatically adds a builder class to the
# registry, using its TARGET_OS and GYP_GENERATOR attributes to determine the
# key.  This is an ordered dict, so the first builder registered for an OS will
# be the default for that OS, and will always be used if the user does not pass
# -g to this script on the command line.
BUILDER_REGISTRY = collections.OrderedDict()


def RegisterBuilder(builder_class):
  """Decorator to register a TargetBuilder class with the registry.

  The class to be registered must define a TARGET_OS attribute, which will be
  used to construct the key in the builder registry.

  Args:
    builder_class: The concrete class to register.

  Returns:
    The class (unmodified).

  Raises:
    Error: if the builder being registered does not define TARGET_OS.
  """
  if not builder_class.TARGET_OS:
    raise Error('Cannot register builder class {0}.  Builder classes must '
                'set the TARGET_OS attribute.'.format(builder_class.__name__))

  builder_os = builder_class.TARGET_OS
  if builder_class.TARGET_FLAVOR:
    builder_os += '-{0}'.format(builder_class.TARGET_FLAVOR)

  key = (builder_os, builder_class.GYP_GENERATOR)
  BUILDER_REGISTRY[key] = builder_class
  return builder_class


@RegisterBuilder
class LinuxBuilder(TargetBuilder):
  """Linux builder."""
  TARGET_OS = 'linux'
  POSSIBLE_HOST_OS = ['linux']
  GYP_GENERATOR = 'ninja'
  GYP_OUT_SUBDIR = 'linux'

  def BuildEnv(self, configuration=None):
    env = super(LinuxBuilder, self).BuildEnv(configuration)
    if self.cmdflags.verbose:
      env['LSBCC_VERBOSE'] = '1'
    return env


@RegisterBuilder
class LinuxHostBuilder(LinuxBuilder):
  """Linux builder for host builds, such as tools.

  This builder exists mainly to distinguish between the build output
  directories, so that the build artifacts (ninja files) for tools don't
  clobber the ninja files from the non-tools builds.
  """
  TARGET_FLAVOR = 'host'
  GYP_OUT_SUBDIR = 'linux-host'


@RegisterBuilder
class ASMJSBuilder(TargetBuilder):
  """ASMJS builder."""
  TARGET_OS = 'asmjs'
  POSSIBLE_HOST_OS = ['linux']
  GYP_GENERATOR = 'ninja'
  GYP_OUT_SUBDIR = 'asmjs'

  def BuildEnv(self, configuration=None):
    env = super(ASMJSBuilder, self).BuildEnv(configuration)
    env['LLVM'] = LLVM_PATH
    env['NODE'] = NODEJS_BINARY_PATH
    return env

  def RunTest(self, configuration, test_target, test_args):
    """Given a configuration and test name, run the test with node.

    Args:
      configuration: the build configuration whose built test we want to run.
      test_target: path (relative or absolute) to a .gyp file, along with the
          target, e.g. 'port/tests/port_test.gyp:port_test'.
      test_args: list of command line arguments for test, or None.

    Returns:
      The multiprocessing.AsyncResult object of the process. Use .get() on this
      to get the return code of the binary.
    """
    test_path = self.PathToBuiltTest(configuration, test_target)

    # Assuming `node` is in the path.
    command_args = [NODEJS_BINARY_PATH, test_path]
    if test_args:
      command_args.append('--')
      command_args.extend(test_args)
    print ' '.join(command_args)
    async_result = self.pool.apply_async(
        _AsyncSubprocess, args=(self.stdout_lock, test_target, command_args,
                                self.cmdflags.verbose))
    return async_result


@RegisterBuilder
class NACLBuilder(TargetBuilder):
  """Native Client builder."""
  TARGET_OS = 'nacl'
  POSSIBLE_HOST_OS = ['linux', 'mac']
  GYP_GENERATOR = 'ninja'
  GYP_OUT_SUBDIR = 'nacl'

  def RunTest(self, configuration, test_target, test_args):
    """Given a configuration and test name, run via the SecureELF Loader.

    Args:
      configuration: the build configuration whose built test we want to run.
      test_target: path (relative or absolute) to a .gyp file, along with the
          target, e.g. 'port/tests/port_test.gyp:port_test'.
      test_args: list of command line arguments for test, or None.

    Returns:
      The AsyncResult object of the process. Use .get() on this to get the
      return code of the binary.
    """
    test_path = self.PathToBuiltTest(configuration, test_target)
    command_args = [sys.executable]
    command_args.append(
        os.path.abspath(os.path.join(NACL_SDK_DIR, 'tools', 'sel_ldr.py')))
    command_args.append(test_path)
    if test_args:
      command_args.append('--')
      command_args.extend(test_args)

    async_result = self.pool.apply_async(
        _AsyncSubprocess, args=(self.stdout_lock, test_target, command_args,
                                self.cmdflags.verbose))
    return async_result


@RegisterBuilder
class PNACLBuilder(NACLBuilder):
  """Portable Native Client builder."""
  # Same target OS, but different flavor.
  TARGET_FLAVOR = 'pnacl'
  GYP_GENERATOR = 'ninja'
  GYP_OUT_SUBDIR = 'nacl-pnacl'

  def RunTest(self, unused_configuration, test_target, unused_test_args):
    _PrintTestHeader(test_target)
    # No way to run pnacl binaries (yet).
    print 'No way to run test, skipping', test_target
    return 0


@RegisterBuilder
class AndroidBuilder(TargetBuilder):
  """Android builder."""
  TARGET_OS = 'android'
  # For historic reasons, TARGET_FLAVOR is not set to 'arm' in this builder,
  # which is the default Android builder. When doing '-o android', the '-arm'
  # part is implicit. We may want to revisit this.

  POSSIBLE_HOST_OS = ['linux', 'mac']
  GYP_GENERATOR = 'ninja-android'
  GYP_OUT_SUBDIR = 'android'

  ANDROID_ARCH = 'arm'
  ANDROID_TOOL_PREFIX = 'arm-linux-androideabi'


@RegisterBuilder
class AndroidArmBuilder(AndroidBuilder):
  """Android arm builder."""
  TARGET_FLAVOR = 'arm'
  GYP_OUT_SUBDIR = 'android-arm'

  ANDROID_ARCH = 'arm'
  ANDROID_TOOL_PREFIX = 'arm-linux-androideabi'


@RegisterBuilder
class AndroidX86Builder(AndroidBuilder):
  """Android x86 builder."""
  TARGET_FLAVOR = 'x86'
  GYP_OUT_SUBDIR = 'android-x86'

  ANDROID_ARCH = TARGET_FLAVOR
  ANDROID_TOOL_PREFIX = 'i686-linux-android'


# Not registered! This is a base class.
class AndroidBuilderNoEmu(AndroidBuilder):
  """A unregistered builder base class that disables emulator testing."""

  def GlobalTestSetup(self):
    pass

  def RunTest(self, unused_configuration, test_target, unused_test_args):
    _PrintTestHeader(test_target)
    # No way to run these binaries for this android (no emulator support).
    print 'No way to run test, skipping', test_target
    return 0


@RegisterBuilder
class AndroidMipsBuilder(AndroidBuilderNoEmu):
  """Android mips builder."""
  TARGET_FLAVOR = 'mips'
  GYP_OUT_SUBDIR = 'android-mips'

  ANDROID_ARCH = TARGET_FLAVOR
  ANDROID_TOOL_PREFIX = 'mipsel-linux-android'


@RegisterBuilder
class AndroidArm64Builder(AndroidBuilderNoEmu):
  """Android ARM64 builder."""
  TARGET_FLAVOR = 'arm64'
  GYP_OUT_SUBDIR = 'android-arm64'

  ANDROID_ARCH = TARGET_FLAVOR
  ANDROID_TOOL_PREFIX = 'aarch64-linux-android'


@RegisterBuilder
class AndroidMips64Builder(AndroidBuilderNoEmu):
  """Android MIPS64 builder."""
  TARGET_FLAVOR = 'mips64'
  GYP_OUT_SUBDIR = 'android-mips64'

  ANDROID_ARCH = TARGET_FLAVOR
  ANDROID_TOOL_PREFIX = 'mips64el-linux-android'


@RegisterBuilder
class Androidx8664Builder(AndroidBuilderNoEmu):
  """Android x86_64 builder."""
  TARGET_FLAVOR = 'x86_64'
  GYP_OUT_SUBDIR = 'android-x86_64'

  ANDROID_ARCH = TARGET_FLAVOR
  ANDROID_TOOL_PREFIX = 'x86_64-linux-android'


@RegisterBuilder
class MacNinjaBuilder(TargetBuilder):
  """Mac builder using ninja."""
  TARGET_OS = 'mac'
  POSSIBLE_HOST_OS = ['mac']
  GYP_GENERATOR = 'ninja'
  GYP_OUT_SUBDIR = 'mac-ninja'


@RegisterBuilder
class MacHybridBuilder(MacNinjaBuilder):
  """Mac builder using Xcode-ninja hybrid."""
  GYP_GENERATOR = 'xcode-ninja'
  GYP_OUT_SUBDIR = 'mac-hybrid'

  def GypArgs(self):
    gyp_args = super(MacHybridBuilder, self).GypArgs()
    gyp_args['--generator-output'] = '{d}'.format(d=GYP_PROJECTS_DIR)
    # Xcode-ninja doesn't handle --suffix correctly. This should be fixed
    # upstream, but for now just work around it.
    del gyp_args['--suffix']
    return gyp_args

  def GypGenerator(self):
    # Hybrid mode requires running two generators: xcode-ninja to create the
    # shell project, and ninja to create the actual build files the shell uses.
    return ['xcode-ninja', 'ninja']

  def CleanGeneratorDirectory(self):
    """Delete the generator files directory (xcode projects)."""
    PrintStatus('Removing ' + GYP_PROJECTS_DIR)
    if os.path.isdir(GYP_PROJECTS_DIR):
      _SmartDeleteDirectory(GYP_PROJECTS_DIR)


@RegisterBuilder
class MacNinjaHostBuilder(MacNinjaBuilder):
  """Mac builder using ninja, for host builds, such as tools.

  This builder exists mainly to distinguish between the build output
  directories, so that the build artifacts (ninja files) for tools don't
  clobber the ninja files from the non-tools builds.
  """
  TARGET_FLAVOR = 'host'
  GYP_OUT_SUBDIR = 'mac-ninja-host'


@RegisterBuilder
class MacBuilder(TargetBuilder):
  """Mac builder."""

  TARGET_OS = 'mac'
  POSSIBLE_HOST_OS = ['mac']
  GYP_GENERATOR = 'xcode'
  GYP_OUT_SUBDIR = 'mac-xcode'

  def GypArgs(self):
    generator_output_dir = GYP_PROJECTS_DIR

    gyp_args = super(MacBuilder, self).GypArgs()
    gyp_args['--generator-output'] = '{d}'.format(d=generator_output_dir)
    gyp_args['-G'] += ['xcode_project_version=3.2']
    return gyp_args

  def BuildCWD(self):
    return GYP_PROJECTS_DIR

  def BuildBinaryPath(self):
    return '/usr/bin/xcodebuild'

  def BuildArgs(self, configuration, filename):
    build_args = []

    if self.cmdflags.keep_going:
      build_args.append('-PBXBuildsContinueAfterErrors=YES')
    if self.cmdflags.verbose:
      build_args.append('-verbose')

    if self.cmdflags.threads is not None:
      # If --threads is not specified, let xcodebuild figure it out itself.
      build_args.append('-jobs={t}'.format(t=self.cmdflags.threads))

    build_args += ['-configuration', configuration]
    project_name, _ = os.path.splitext(filename)
    build_args += ['-project', '{p}_{o}.xcodeproj'.format(p=project_name,
                                                          o=self.TARGET_OS)]
    return build_args

  def PathToBuiltTest(self, configuration, target):
    """Return the absolute path to the built target.

    Args:
      configuration: the build configuration whose built test we want to run.
      target: path (relative or absolute) to a .gyp file, along with the target,
              e.g. 'port/tests/port_test.gyp:port_test'.

    Returns:
      Absolute path to the target on disk.
    """
    target_name = target.split(':')[-1]
    path = os.path.join(
        self.BuildOutputRootDir(), configuration, 'obj', configuration,
        target_name)
    return path

  def CleanGeneratorDirectory(self):
    """Delete the generator files directory (xcode projects)."""
    PrintStatus('Removing ' + GYP_PROJECTS_DIR)
    if os.path.isdir(GYP_PROJECTS_DIR):
      _SmartDeleteDirectory(GYP_PROJECTS_DIR)


@RegisterBuilder
class IOSBuilder(MacBuilder):
  """Builder for iOS, using xcode."""
  TARGET_OS = 'ios'
  POSSIBLE_HOST_OS = ['mac']
  GYP_OUT_SUBDIR = 'ios'

  SDK = 'iphoneos'

  def BuildArgs(self, configuration, filename):
    build_args = super(IOSBuilder, self).BuildArgs(
        configuration, filename)

    build_args.extend(['-sdk', self.SDK])
    return build_args

  def PathToBuiltTest(self, configuration, target):
    """Return the absolute path to the built test iOS bundle.

    Args:
      configuration: the build configuration whose built test we want to run.
      target: path (relative or absolute) to a .gyp file, along with the target,
              e.g. 'port/tests/port_test.gyp:port_test'.

    Returns:
      Absolute path to the bundle on disk.
    """
    target_name = target.split(':')[-1]
    path = os.path.join(
        self.BuildOutputRootDir(), configuration, 'obj',
        configuration + '-' + self.SDK, target_name + '.app')
    return path


@RegisterBuilder
class IOSSimulatorBuilder(IOSBuilder):
  """Builder for iOS simulator, using xcode."""
  TARGET_OS = 'ios'
  TARGET_FLAVOR = 'x86'  # simulator
  POSSIBLE_HOST_OS = ['mac']
  GYP_OUT_SUBDIR = 'ios-x86'

  SDK = 'iphonesimulator'


@RegisterBuilder
class IOSNinjaBuilder(MacNinjaBuilder):
  """Builder for iOS, using ninja."""
  TARGET_OS = 'ios'
  POSSIBLE_HOST_OS = ['mac']
  GYP_OUT_SUBDIR = 'ios-ninja'

  def PathToBuiltTest(self, configuration, target):
    """Return the absolute path to the built test iOS bundle.

    Args:
      configuration: the build configuration whose built test we want to run.
      target: path (relative or absolute) to a .gyp file, along with the target,
              e.g. 'port/tests/port_test.gyp:port_test'.

    Returns:
      Absolute path to the bundle on disk.
    """
    target_name = target.split(':')[-1]
    path = os.path.join(
        self.BuildOutputRootDir(), configuration, target_name + '.app')
    return path


@RegisterBuilder
class IOSNinjaSimulatorBuilder(IOSNinjaBuilder):
  """Builder for iOS simulator, using ninja."""
  TARGET_OS = 'ios'
  TARGET_FLAVOR = 'x86'  # simulator
  POSSIBLE_HOST_OS = ['mac']
  GYP_OUT_SUBDIR = 'ios-x86-ninja'


@RegisterBuilder
class WindowsBuilderNinja(TargetBuilder):
  """Windows builder for ninja."""

  TARGET_OS = 'win'
  POSSIBLE_HOST_OS = ['win']
  GYP_GENERATOR = 'ninja'
  GYP_OUT_SUBDIR = 'win-ninja'

  def GypArgs(self):
    gyp_args = super(WindowsBuilderNinja, self).GypArgs()
    # Ninja on windows needs some extra information.
    gyp_args['-G'] += ['ninja_use_custom_environment_files']

    # The ninja generator, when run on windows, has a bug that fails to pass our
    # environment variables (such as GYP_CROSSCOMPILE) to gyp.  The
    # multiprocessing module on windows can't use fork(2) (b/c it doesn't exist
    # there) so each "sub"process is launched with the environment its parent
    # had at startup, therefore not reflecting subsequent modifications of that
    # environment. See:
    # https://docs.python.org/2/library/multiprocessing.html#windows
    gyp_args['--no-parallel'] = None
    return gyp_args

  def GypEnv(self):
    gyp_env = super(WindowsBuilderNinja, self).GypEnv()

    # To get proper host toolchain support on windows, this is necessary,
    # because otherwise the generated ninja files are confused about host
    # toolchain linking.
    gyp_env['AR_host'] = 'lib.exe'

    # The GYP_MSVS_OVERRIDE_PATH doesn't matter, as all the toolchain paths are
    # set in dev/windows.gypi and dev/gen_ninja_environment.py.
    gyp_env['GYP_MSVS_OVERRIDE_PATH'] = 'some_dummy_value_doesnt_matter'

    # This is important, however, and it must match the toolchain in the above
    # mentioned files.
    gyp_env['GYP_MSVS_VERSION'] = '2013'

    return gyp_env

  def GypDefines(self):
    gyp_defines = super(WindowsBuilderNinja, self).GypDefines()
    # Here comes a lesson in gyp+ninja on windows: to find the compiler
    # binaries, ninja uses 'environment' files, e.g. 'environment.x86' for the
    # x86 toolchain. Ninja also assumes that these files exist at the top of the
    # configuration directory, e.g. gyp-out/windows/dbg_x86/environment.x86.
    #
    # The catch is that these files must exist *before* any building can be
    # done. Other projects wrap up the creation of these files in their build.py
    # or equivalent. We've put the creation of these files into gyp (see
    # generate_ninja_environment.gyp) for a more sensical implementation.
    #
    # The catch is that we have to know at gyp-time the list of available
    # configurations, because each configuration needs that file to exist. There
    # is no way in gyp to "know" the available configurations that have been
    # defined, so we make this slight concession here: we pass in the list of
    # available windows configurations.
    gyp_defines['windows_possible_configurations'] = ' '.join(
        self.state.GetConfigurationsForOS(self.TARGET_OS,
                                          generator=self.GYP_GENERATOR))
    return gyp_defines


@RegisterBuilder
class WindowsHostBuilderNinja(WindowsBuilderNinja):
  """Windows builder for host builds, such as tools.

  This builder exists mainly to distinguish between the build output
  directories, so that the build artifacts (ninja files) for tools don't
  clobber the ninja files from the non-tools builds.
  """
  TARGET_FLAVOR = 'host'
  GYP_OUT_SUBDIR = 'win-ninja-host'


@RegisterBuilder
class WindowsBuilderMSVS(TargetBuilder):
  """Windows builder for MSVS projects."""
  TARGET_OS = 'win'
  POSSIBLE_HOST_OS = ['win']
  GYP_GENERATOR = 'msvs'
  GYP_OUT_SUBDIR = 'win-msvs'

  def GypArgs(self):
    gyp_args = super(WindowsBuilderMSVS, self).GypArgs()
    gyp_args['--generator-output'] = '{d}'.format(d=GYP_PROJECTS_DIR)
    return gyp_args

  def RunBuild(self, unused_configuration, filename):
    """Point to the solution files for visual studio.

    Args:
      unused_configuration: The gyp configuration (unused).
      filename: gyp filename of the target of interested.
    """
    print 'Open Visual Studio to build:'
    print

    # Point to the .sln file that we just generated.
    project_name, _ = os.path.splitext(filename)
    print os.path.join(GYP_PROJECTS_DIR,
                       '{p}_{o}.sln'.format(p=project_name, o=self.TARGET_OS))

  def CleanGeneratorDirectory(self):
    """Delete the generator files directory (MSVS projects)."""
    PrintStatus('Removing ' + GYP_PROJECTS_DIR)
    if os.path.isdir(GYP_PROJECTS_DIR):
      _SmartDeleteDirectory(GYP_PROJECTS_DIR)


# This is not decorated with @RegisterBuilder because it only implements some
# of the builder logic, so we don't want it to show up in the OS/configuration
# mapping.  It is created and used directly if --deps is passed to this script.
class DumpDependencyBuilder(TargetBuilder):
  """Dependency builder.

  This is not really a builder in that it doesn't do any building, but does
  produce a full dependency json file for the given gyp file.

  As such, it has no TARGET_OS, so this needs to be set to the correct value
  (the actual target OS that you are interested in generating a dependency json
  file for).
  """

  # TARGET_OS is deliberately unset.
  TARGET_OS = None
  # Any host OS is possible.
  POSSIBLE_HOST_OS = ['linux', 'mac', 'win']
  GYP_GENERATOR = 'dump_dependency_json'
  # This is unused, but a value is needed.
  GYP_OUT_SUBDIR = 'json'

  def __init__(self, state, target_os=None, target_flavor=''):
    assert target_os is not None
    # pylint: disable=invalid-name
    self.TARGET_OS = target_os
    self.TARGET_FLAVOR = target_flavor
    super(DumpDependencyBuilder, self).__init__(state)

  def RunGyp(self, filename):
    # The dump_dependency_json gyp generator has a bug in that it does not
    # create its parent directories before attempting to write dump.json. This
    # is to work around that bug:
    if not os.path.isdir(self.BuildOutputRootDir()):
      os.makedirs(self.BuildOutputRootDir())

    super(DumpDependencyBuilder, self).RunGyp(filename)

  def GetDependencyJson(self, filename):
    self.RunGyp(filename)
    json_path = os.path.join(self.BuildOutputRootDir(), 'dump.json')
    if not os.path.exists(json_path):
      json_path = os.path.join(self.GypCWD(), 'dump.json')
    return json.load(open(json_path, 'r'))

  def RunBuild(self, unused_configuration, unused_filename):
    """Undefined, do not call."""
    raise NotImplementedError


class BuildState(object):
  """Stores, computes, and retrieves various build-related state.

  The public interface of BuildState consists solely of accessor methods.
  These all return cached data which is computed when the BuildState object is
  constructed, so they are very inexpensive to call.
  """

  def __init__(self, argv):
    """Initializes a BuildState object.

    When a BuildState object is initialized, argv is parsed for command-line
    flags and positional parameters.  If invalid or unknown flags are passed,
    script execution may halt there without returning back to main.  Otherwise,
    all the accessor methods may then be used to get information about the build
    state, such as the command-line flags, gyp file to build, available OSes
    and configurations, etc.

    Args:
      argv: List of command-line parameters, directly as obtained from sys.argv
          (so the first element should be the script itself).
    """
    # Build OS is the OS of the machine this script is being run on.
    try:
      self.host_os = GetHostOS()
    except KeyError:

      ExitWithError('Unknown build OS returned by platform.system(): {0}'.
                    format(platform.system()))

    # Determine default target OS, i.e. the OS of the machine the target binary
    # will run on.
    self.default_target_os_ = {
        'mac': 'mac',
        'linux': 'linux',
        'win': 'win',
    }[self.host_os]

    self.args_ = self._ParseCommandLineArgs(argv)

  def GetCommandLineOptions(self):
    """Returns an object with attributes for each command line option.

    Returns:
      An argparse.Namespace object.
    """
    return self.args_

  def GetPossibleBuildFlags(self):
    """Returns all the build flags that can be set on the command line.

    The build flags are pre-parsed when the BuildState is constructed, so
    this method just returns the existing dict and is inexpensive to call.

    Returns:
      A dictionary whose keys are all possible build flags for this build, and
      whose values are the default values for those flags.
    """
    return self.possible_build_flags_

  def GetActiveBuildFlags(self):
    """Returns the active build flags that were set on the command line.

    A build flag is active if it is specified on the command line AND its value
    as specified is different from the default value found in
    common_variables.gypi.

    Returns:
      A dictionary whose keys are the active build flags for this build, and
      whose values are the active settings for those flags.
    """
    # Of the possible gyp variables, find the ones that were set on the command
    # line to some non-default value.  Technically we ought to do this eagerly
    # and cache it for this to be a true getter, but it's all in-memory, so it's
    # pretty cheap, and in practice we currently only call it once anyway.
    variables = {}
    args = self.GetCommandLineOptions()
    for k, v in self.GetPossibleBuildFlags().items():
      if v != getattr(args, k):
        variables[k] = getattr(args, k)
    return variables

  def GetAdditionalGypVariables(self):
    """Returns list of additional gyp variables.

    These are not defined via GetActiveBuildFlags, and instead are directly
    passed on the command line as -D=foo=bar.

    Returns:
      Dictionary whose keys are gyp variable names and values are their values.
    """
    variables = {}
    args = self.GetCommandLineOptions()
    variables.update([arg.split('=') for arg in args.D])
    return variables

  def GetAdditionalGypGeneratorFlags(self):
    """Returns list of additional gyp variables.

    These are directly passed on the command line as -G=foo=bar.

    Returns:
      A list of extra flags to pass through to the gyp as generator flags.
    """
    return self.GetCommandLineOptions().G

  def GetGypFileToRun(self):
    """Returns the gypfile that should be passed to gyp.

    This method always returns a relative path from ROOT_DIR, because gyp
    requires that the gypfile be specified relative to <(DEPTH) in order for
    --generator_output_dir to work correctly, and we currently pass
    --depth=ROOT_DIR.

    Returns:
      The relative path from <(DEPTH) to the gypfile.
    """
    return self.filename_

  def GetConfigurationsForOS(self, target_os, target_flavor='', generator=None):
    """Returns list of gyp configurations available for a target OS and flavor.

    Unlike _FindConfigurationsForOS, this method returns cached data and is
    very inexpensive.

    Args:
      target_os: The target OS to get configurations for.
      target_flavor: The target flavor to get configurations for, if any.
      generator: The generator to use for this target os/flavor.

    Returns:
      List of configuration names available for the given arguments.
    """
    return self.all_configs_[(target_os, target_flavor, generator)]

  @classmethod
  def _FindAllConfigurations(cls):
    """Return all the configurations for each builder."""
    all_configs = {}
    for builder in BUILDER_REGISTRY.values():
      key = (builder.TARGET_OS, builder.TARGET_FLAVOR, builder.GYP_GENERATOR)
      all_configs[key] = cls._FindConfigurationsForOS(*key)
    return all_configs

  @classmethod
  def _FindConfigurationsForOS(cls, target_os, target_flavor='', generator=''):
    """Computes list of gyp configurations available for a target OS and flavor.

    Minimally parses os.gypi to find the valid configuration names for the given
    target OS and (optional) flavor.  If os.gypi does not exist, returns an
    empty list.

    Note:
      Currently, this method is hard-coded to look for os.gypi in ION_DIR.  This
      is probably fine for any project that uses Ion, but this script aims to be
      generic, so this may change in the future.

    Args:
      target_os: The target OS to find configurations for.
      target_flavor: The target flavor to find configurations for, if any.
      generator: The generator being used, if specified.

    Returns:
      List of configuration names available for the given arguments, with the
      'default_configuration' (if specified) being the first in the list.
    """
    default_configuration = None
    configuration_names = []

    os_gypi_file = os.path.join(ION_DIR, CONFIGURATION_GYPI)
    if not os.path.isfile(os_gypi_file):
      return []

    with open(os_gypi_file, 'r') as os_gypi:
      root_dict = eval(os_gypi.read())

      # All other variables/globals are not defined and will cause an evaluation
      # error if they are seen in os.gypi.
      gyp_globals = {
          'OS': target_os,
          'flavor': target_flavor,
          'GENERATOR': generator,
          '__builtins__': None
      }

      # Poor man's gyp processing: we are only interested in finding
      # 'configurations', so parse and process the resulting top-level dict,
      # descending as needed, and collecting configuration names. The below only
      # handles 'target_defaults', 'conditions', and 'configurations' sections,
      # which should suffice.
      dicts_to_eval = [root_dict]
      while dicts_to_eval:
        current_dict = dicts_to_eval.pop(0)

        for k, v in current_dict.items():
          if k == 'target_defaults':
            # Although technically the 'configurations' section can also be
            # found inside a 'targets', we have adopted the convention that they
            # only appear in our os.gypi file, inside the 'target_defaults'.
            # Therefore, it is fine to only descend into this section.
            dicts_to_eval.append(v)
          elif k == 'conditions':
            for condition in v:
              if len(condition) == 2:
                predicate, true_dict = condition
                else_dict = {}
              else:
                predicate, true_dict, else_dict = condition

              if eval(predicate, gyp_globals):
                dicts_to_eval.append(true_dict)
              elif else_dict:
                dicts_to_eval.append(else_dict)
          elif k == 'configurations':
            # Add the configuration names to the list, iff they are not
            # abstract.
            configuration_names.extend(conf for conf, values in v.items()
                                       if not values.get('abstract', False))
          elif k == 'default_configuration':
            default_configuration = v

    # The list has no specific ordering thus far, because dicts are unordered.
    # Apply an ordering, just so that the return values are somewhat consistent.
    configuration_names.sort()

    # If there's a default_configuration, make it first in the list.
    if default_configuration in configuration_names:
      configuration_names.remove(default_configuration)
      configuration_names.insert(0, default_configuration)
    # Skip the error case where default_configuration refers to a undefined
    # configuration.

    return configuration_names

  @classmethod
  def _FindBuildFlags(cls, filename):
    """Parses gypi files to construct a list of build flags.

    Minimally parse common_variables.gypi to find the available variables.
    Descends 'includes' files as well.  After a BuildState object has been
    constructed, you can retrieve this list without re-computing it by calling
    its GetPossibleBuildFlags() method.

    If common_variables.gypi is not found, returns an empty dict.

    Args:
      filename: The path to the gypfile being built.  This is used as an anchor
          for searching for the common_variables.gypi file.  It must be relative
          to ROOT_DIR.

    Returns:
      Dict of variables that are possible to set, and their default value (as it
      exists in common_variables.gypi).
    """

    variables = {}
    files_to_parse = []

    common_variables = _FindNearestFiles(filename, COMMON_VARIABLES)
    if common_variables:
      files_to_parse.append(common_variables)

    while files_to_parse:
      f = files_to_parse.pop(0)
      if not os.path.isfile(f):
        continue
      with open(f, 'r') as gypi:
        root_dict = eval(gypi.read())

        # Poor man's gyp processing: we are only interested in finding
        # 'variables'.
        dicts_to_eval = [root_dict]
        while dicts_to_eval:
          current_dict = dicts_to_eval.pop(0)

          for k, v in current_dict.items():
            if k == 'variables':
              for i, j in v.items():
                # Only consider variables with a '%' sign at the end, meaning
                # that they will take a value from the command line if present,
                # and also a simple default value (those containing '<' contain
                # a variable expansion).
                if i[-1:] == '%' and '<' not in str(j):
                  variables[i[:-1]] = j
                elif i == 'variables':
                  # Parse this 'variables' entry as a dictionary of its own.
                  dicts_to_eval.append({i: j})

            elif k == 'includes':
              files_to_parse.extend(
                  [os.path.join(os.path.dirname(f), include) for include in v])

    return variables

  @classmethod
  def _MakeGypFilePath(cls, path_string):
    """Returns a gypfile path from a path-like specification string.

    Gyp wants paths to individual gypfiles to build, but that's sometimes
    annoying.  This function generates such paths from a number of different
    path-like strings, including bazel-style path specs.  All of the following
    should return "path/to/gypfile.gyp" if this script is invoked from the main
    root directory:

    path/to/gypfile.gyp
    path/to
    //path/to

    Note that colons in specs are not yet supported, because they represent
    individual targets, which we cannot yet select.

    Args:
      path_string: A path-like string describing a gyp file to run.

    Returns:
      A relative path to the gypfile represented by the path string.  If the
      gypfile is fully specified in path_string but does not exist, returns
      path_string directly.  If path_string represents a directory not
      containing a gypfile with the same name, returns None.
    """
    if path_string is None:
      return None

    # Resolve '//' as a path relative to the root dir, regardless of the current
    # working directory.
    if path_string.startswith('//'):
      path_string = path_string[2:]
    else:
      path_string = os.path.relpath(os.path.abspath(path_string), ROOT_DIR)

    if not path_string.endswith('.gyp'):
      # Assume this is a path to the containing directory and infer the gypfile.
      default_gypfile = cls._GetDefaultGypFile(path_string)
      if default_gypfile:
        return default_gypfile
      else:
        return None
    return path_string

  @classmethod
  def _GetDefaultGypFile(cls, working_directory):
    """Returns the default gypfile to build for the given directory.

    This function returns a path to a gypfile at the top level of
    working_directory whose name is the same as the directory's.  So for
    example, if working_directory is "/path/to/ion", it will return
    "/path/to/ion/ion.gyp".  This path is returned even if it does not actually
    exist in the file system.

    Args:
      working_directory: The directory that the default gypfile should be
          returned for.

    Returns:
      The gypfile that should be built by default for the given directory.
    """
    working_directory = working_directory.rstrip(os.sep)
    barename = os.path.basename(working_directory)
    return os.path.join(working_directory, '{0}.gyp'.format(barename))

  def _ParseCommandLineArgs(self, argv):
    """Parses command line arguments and shows help/usage if necessary.

    This function wraps some advanced usage of argparse.parser.  We use a two-
    pass parsing strategy: the first pass populates all of the options and
    arguments that aren't dependent on any other options (which is most of
    them). The second pass uses the information gained from the first pass to
    determine what other options (such as configuration names and build flags,
    which depend on what gypfile is being built) should be available, and parses
    those as well.

    Args:
      argv: The full command line, including the name of the script in argv[0].

    Returns:
      A 2-tuple consisting of a dictionary of command-line options and their
      values, followed by a list of positional arguments.  (This is the same
      tuple returned by argparse.ArgumentParser.parse_args.)
    """
    # Load the registry of builders that have been annotated with
    # @RegisterBuilder and determine what OSes and configurations we know how to
    # build.  This information is displayed if the --help option is found.
    available_builds = []
    unavailable_builds = []

    this_os = GetHostOS()

    self.all_configs_ = self._FindAllConfigurations()
    for key, builder in BUILDER_REGISTRY.items():
      target_os, generator = key
      can_build = this_os in builder.POSSIBLE_HOST_OS
      if can_build:
        style = colorama.Style.RESET_ALL
      else:
        style = colorama.Style.DIM

      if generator:
        os_and_configs = '{2}{0} ({1}): '.format(target_os, generator, style)
      else:
        os_and_configs = '{1}{0}: '.format(target_os, style)

      os_and_configs += ', '.join(
          self.all_configs_[builder.TARGET_OS, builder.TARGET_FLAVOR,
                            builder.GYP_GENERATOR])
      if can_build:
        available_builds.append(os_and_configs)
      else:
        unavailable_builds.append(os_and_configs)
    all_builds = available_builds + unavailable_builds

    # First pass: add options whose presence or default value doesn't depend on
    # any other arguments.  We create the parser without a --help option,
    # because if we added one and the user passed the option, ArgumentParser
    # would print usage and exit before performing the second pass.
    parser = argparse.ArgumentParser(
        epilog=('\nPossible OS and configuration values:\n  ' +
                '\n  '.join(all_builds)) + '\n',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        add_help=False)
    parser.add_argument(
        '-o', '--os',
        default=self.default_target_os_,
        help='The target OS to build for.')
    parser.add_argument(
        '-j', '--threads', default=None,
        help='How many threads to use for the build process.')
    parser.add_argument(
        '--ninja', action='store_true',
        default=False,
        help='Whether to use ninja instead of the native build tool.  If '
        'passed, this is equivalent to -g ninja.')
    parser.add_argument(
        '-k', '--keep-going', action='store_true',
        default=False,
        help='Continue building after errors.')
    parser.add_argument(
        '--nogyp', action='store_true',
        default=False,
        help='If true, will not generate project files, only build. Use '
        'this to save a couple of seconds if you know your gyp files have '
        'not been modified.')
    parser.add_argument(
        '--nobuild', action='store_true',
        default=False,
        help='If true, will not build binaries, only the project files.')
    parser.add_argument(
        '--clean', action='store_true',
        default=False,
        help='If true, clean the build directory and generator files ('
        'gyp-out/$os and gyp-projects).')
    parser.add_argument(
        '--gypd', action='store_true',
        default=False,
        help='Whether to output a .gypd debug file, and exit. '
        '(Implies --nobuild).')
    parser.add_argument(
        '--deps', action='store_true',
        default=False,
        help='Whether to output the dependency graph in JSON format to '
        'dump.json, and exit. (Implies --nobuild).')
    parser.add_argument(
        '-w', '--verbose', action='store_true',
        default=False,
        help='Whether to invoke the build in verbose mode.')
    parser.add_argument(
        '-t', '--test', action='store_true',
        default=False,
        help='Run all tests.')
    parser.add_argument(
        '-D', action='append',
        default=[],
        help='Additional values to pass to gyp. Use as: -D=my_var=my_value')
    parser.add_argument(
        '--test_arg', action='append',
        default=None,
        help='Specifies a flag to pass to the test.  For example, to pass a '
        'filter to gunit use --test_arg=--gunit_filter=Example.TestCase.  '
        'To pass multiple flags use --test_arg multiple times.')
    parser.add_argument(
        '-T', '--test_until_failure', action='store_true',
        dest='test_until_failure',
        default=False,
        help='Run all tests, until the first failing test.')
    parser.add_argument(
        '-c', '--configuration',
        default=None,
        help='What configuration to build. See below for available '
        'configurations.')
    parser.add_argument(
        '-g', '--generator',
        default=None,
        help='What gyp generator to use.  If not specified, uses the default '
        'generator for the target OS.')
    parser.add_argument(
        '-G', action='append',
        default=[],
        help='Generator flags to pass to gyp.')
    parser.add_argument(
        'path',
        nargs='?',
        default=None,
        help='The gypfile or package path you wish to build.')

    # Parse only known args, since project-specific build flags will be
    # determined later.
    args = parser.parse_known_args(argv[1:])

    # Second pass: calculate the path to the gyp file the user wants to build,
    # and parse it looking for project-specific build flags we should add
    # options for.
    if args and args[0].path:
      self.filename_ = self._MakeGypFilePath(args[0].path)
    elif os.path.isfile(os.path.join(ROOT_DIR,
                                     self._MakeGypFilePath(os.getcwd()))):
      self.filename_ = self._MakeGypFilePath(os.getcwd())
    else:
      self.filename_ = None

    self.possible_build_flags_ = {}
    if self.filename_:
      self.possible_build_flags_ = self._FindBuildFlags(self.filename_)

    if self.possible_build_flags_:
      # We'll add the build flags to this group as options in a moment.
      build_flags = parser.add_argument_group('build flags')
    elif not self.filename_:
      # The user didn't give us a gyp file, so we can only show generic help.
      build_flags = parser.add_argument_group(
          'build flags', 'To see project-specific flags, specify a gyp file.')
    else:
      # The gyp file was valid, but contained no build flags.
      build_flags = parser.add_argument_group(
          'build flags', 'No project-specific build flags are defined.')

    # Add a new command-line option for each build flag we found.
    for variable, default_value in self.GetPossibleBuildFlags().items():
      # Default values that can be converted to integers are treated as such
      # and other values (strings) are left as strings. Initially, all values
      # were converted to integers but this would cause an exception if a
      # non-int string was used as a default.
      try:
        default_converted = int(default_value)
      except ValueError:
        default_converted = default_value
      build_flags.add_argument(
          '--' + variable,
          default=default_converted,
          help='(default: %(default)s)')

    # Add the help option now that our option list is complete.
    parser.add_argument('-h', '--help', action='help')

    # Now parse all args, since if the user passed something we don't recognize
    # by this point, it's not a valid option.
    return parser.parse_args(argv[1:])


def GetHostOS():
  return {
      'Darwin': 'mac',
      'Linux': 'linux',
      'Windows': 'win',
  }[platform.system()]


def ExitWithError(message):
  print >> sys.stdout, '{1}ERROR:{2} {0}'.format(
      message, colorama.Fore.RED, colorama.Fore.RESET)
  sys.exit(1)


def PrintStatus(message):
  print '{green}INFO: {reset}{m}'.format(
      m=message, green=colorama.Fore.GREEN, reset=colorama.Fore.RESET)


def main(argv):
  if sys.stdout.isatty():
    colorama.init(autoreset=True)
  else:
    # Don't call colorama if stdout is actually StringIO or cStringIO.
    # It wraps sys.stdout with a class that has only a |write| method.
    stdout_is_stringio = getattr(sys.stdout, 'getvalue', None)
    if not stdout_is_stringio:
      # Strip out all color codes, effectively turning color off.
      colorama.init(strip=True, convert=False)

  state = BuildState(argv)
  args = state.GetCommandLineOptions()

  filename = state.GetGypFileToRun()

  if not filename and not args.clean:
    ExitWithError('Could not determine what you want to build. Try '
                  'specifying a gyp file or directory containing one.')

  gyp_generator = args.generator
  if args.ninja:  # Special case for --ninja.
    gyp_generator = 'ninja'

  # Look for the appropriate builder, and instantiate the builder class.
  builder = None
  key = (args.os, gyp_generator)
  if key in BUILDER_REGISTRY:
    # We have an explicit builder set for this OS+generator combination.
    builder = BUILDER_REGISTRY[key](state)
  elif not args.generator:  # User wants to use the default generator.
    # We didn't have a builder for this OS+generator combination.  Look for any
    # builders for this OS and take the first one we find.
    for key in BUILDER_REGISTRY.iterkeys():
      if key[0] == args.os:
        builder = BUILDER_REGISTRY[key](state)
        break
  else:
    ExitWithError('No builder was found for {0} that uses the {1} generator. '
                  'Use --help to list available build types.'
                  .format(args.os, args.generator))

  if not builder:
    # Fallback failed because there aren't any builders for this target OS.
    ExitWithError('No builder was found for {0}. '
                  'Use --help to list available build types.'.format(args.os))

  if args.deps:
    PrintStatus('Running gyp in deps mode...')
    dump_dependency_json = DumpDependencyBuilder(
        state, target_os=builder.TARGET_OS, target_flavor=builder.TARGET_FLAVOR)
    dump_dependency_json.RunGyp(filename)
    return

  try:
    if args.clean:
      builder.CleanBuildOutputDirectory()
      builder.CleanGeneratorDirectory()
      if not filename:
        # Nothing more to do, since no build target was specified.
        return 0

    if not args.nogyp:
      PrintStatus('Running gyp...')
      gyp_retcode = builder.RunGyp(filename)
      if gyp_retcode:
        ExitWithError('gyp returned non-zero')

    configuration = args.configuration

    if configuration is None:
      # A configuration was not given in the options; use the default.
      configuration = state.GetConfigurationsForOS(
          builder.TARGET_OS, builder.TARGET_FLAVOR, builder.GYP_GENERATOR)[0]

    if not args.nobuild:
      PrintStatus('Building...')
      exitcode = builder.RunBuild(configuration, filename)
      if exitcode:
        ExitWithError('Build failed.')
      else:
        PrintStatus('Build succeeded.')

    if args.test or args.test_until_failure:
      PrintStatus('Testing...')
      retcode = builder.RunTests(configuration, filename,
                                 test_args=args.test_arg,
                                 stop_on_failure=args.test_until_failure)
      if retcode:
        ExitWithError('There were test failures.')
      else:
        PrintStatus('All tests passed.')
  except Error as err:
    ExitWithError(err)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
