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

// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 **********************************************************************
 *   Copyright (C) 2003, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#ifndef __LXUTILITIES_H

#define __LXUTILITIES_H

#include "third_party/iculehb/src/src/LETypes.h"

namespace iculx {

class LXUtilities
{
public:
    static le_int8 highBit(le_int32 value);
    static le_int32 search(le_int32 value, const le_int32 array[], le_int32 count);
    static void reverse(le_int32 array[], le_int32 count);
    static void reverse(float array[], le_int32 count);
};

}  // namespace iculx
#endif
