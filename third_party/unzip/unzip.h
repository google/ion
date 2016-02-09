/*
  Copyright 2015 Google Inc. All Rights Reserved.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  The below is based on the proposed iomem_simple package found at
  http://code.trak.dk/

  Local modifications exist to fix various security bugs and to work with the
  most recent version of unzip.
*/

#ifndef UTIL_UNZIP_UNZIP_H_
#define UTIL_UNZIP_UNZIP_H_

#include "third_party/zlib/src/contrib/minizip/unzip.h"

#ifdef __cplusplus
extern "C" {
#endif

ZEXTERN unzFile ZEXPORT unzAttach OF((voidpf stream, zlib_filefunc_def*));
ZEXTERN voidpf ZEXPORT unzDetach OF((unzFile*));
ZEXTERN voidpf ZEXPORT mem_simple_create_file
OF((zlib_filefunc_def * pzlib_filefunc_def, voidpf buffer, size_t buflen));

#ifdef __cplusplus
}
#endif

#endif  // UTIL_UNZIP_UNZIP_H_
