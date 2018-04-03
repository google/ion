#!/bin/sh
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

# This shell script applies the replace_strings python script to provide simple
# macro substitution for doxygen input files. The input file is named on the
# command line; output goes to standard output.

# These are the tokens that are replaced:
declare -A tokens
tokens["CODE_URL"]="https://github.com/google/ion/tree/master/ion"
tokens["ION_URL"]="https://github.com/google/ion"
tokens["ION_UG_URL"]="https://google.github.io/ion/_users_guide.html"

# Set up the arguments for replace_strings.py:
declare args="--input $1"
for token in "${!tokens[@]}"; do
  args+=" --from "
  args+="{"$token"}"
  args+=" --to "
  args+=${tokens["$token"]}
done

# Invoke the python script.
python ./dev/replace_strings.py $args
