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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/zlib/src/contrib/minizip/ioapi.h"
#include "third_party/zlib/src/contrib/minizip/unzip.c"

ZEXTERN voidpf ZEXPORT unzDetach(file) unzFile* file;
{
  voidpf stream;
  unz64_s* s;
  if (*file == NULL) return NULL;
  s = (unz64_s*)*file;

  if (s->pfile_in_zip_read != NULL) unzCloseCurrentFile(*file);
  stream = s->filestream;
  TRYFREE(s);
  *file = NULL;
  return stream;
}

ZEXTERN unzFile ZEXPORT unzAttach(stream, pzlib_filefunc_def) voidpf stream;
zlib_filefunc_def* pzlib_filefunc_def;
{
  unz64_s us;
  unz64_s* s;
  ZPOS64_T central_pos;
  uLong uL;

  uLong number_disk;         /* number of the current dist, used for
                                spaning ZIP, unsupported, always 0*/
  uLong number_disk_with_CD; /* number the the disk with central dir, used
                                for spaning ZIP, unsupported, always 0*/
  uLong number_entry_CD;     /* total number of entries in
                                the central dir
                                (same than number_entry on nospan) */

  int err = UNZ_OK;

  if (unz_copyright[0] != ' ') return NULL;

  fill_zlib_filefunc64_32_def_from_filefunc32(&us.z_filefunc,
                                              pzlib_filefunc_def);

  us.filestream = stream;
  if (us.filestream == NULL) return NULL;

  central_pos = unz64local_SearchCentralDir(&us.z_filefunc, us.filestream);
  if (central_pos == 0) err = UNZ_ERRNO;

  if (ZSEEK64(us.z_filefunc, us.filestream, central_pos,
              ZLIB_FILEFUNC_SEEK_SET) != 0)
    err = UNZ_ERRNO;

  /* the signature, already checked */
  if (unz64local_getLong(&us.z_filefunc, us.filestream, &uL) != UNZ_OK)
    err = UNZ_ERRNO;

  /* number of this disk */
  if (unz64local_getShort(&us.z_filefunc, us.filestream, &number_disk) !=
      UNZ_OK)
    err = UNZ_ERRNO;

  /* number of the disk with the start of the central directory */
  if (unz64local_getShort(&us.z_filefunc, us.filestream,
                          &number_disk_with_CD) != UNZ_OK)
    err = UNZ_ERRNO;

  /* total number of entries in the central dir on this disk */
  if (unz64local_getShort(&us.z_filefunc, us.filestream, &uL) !=
      UNZ_OK)
    err = UNZ_ERRNO;
  us.gi.number_entry = uL;

  /* total number of entries in the central dir */
  if (unz64local_getShort(&us.z_filefunc, us.filestream, &number_entry_CD) !=
      UNZ_OK)
    err = UNZ_ERRNO;

  if ((number_entry_CD != us.gi.number_entry) || (number_disk_with_CD != 0) ||
      (number_disk != 0))
    err = UNZ_BADZIPFILE;

  /* size of the central directory */
  if (unz64local_getLong(&us.z_filefunc, us.filestream, &uL) !=
      UNZ_OK)
    err = UNZ_ERRNO;
  us.size_central_dir = uL;

  /* offset of start of central directory with respect to the
        starting disk number */
  if (unz64local_getLong(&us.z_filefunc, us.filestream, &uL) !=
      UNZ_OK)
    err = UNZ_ERRNO;
  us.offset_central_dir = uL;

  /* zipfile comment length */
  if (unz64local_getShort(&us.z_filefunc, us.filestream, &us.gi.size_comment) !=
      UNZ_OK)
    err = UNZ_ERRNO;

  if ((central_pos < us.offset_central_dir + us.size_central_dir) &&
      (err == UNZ_OK))
    err = UNZ_BADZIPFILE;

  if (err != UNZ_OK) {
    return NULL;
  }

  us.byte_before_the_zipfile =
      central_pos - (us.offset_central_dir + us.size_central_dir);
  us.central_pos = central_pos;
  us.pfile_in_zip_read = NULL;
  us.encrypted = 0;

  s = (unz64_s*)ALLOC(sizeof(unz64_s));
  *s = us;
  unzGoToFirstFile((unzFile)s);
  return (unzFile)s;
}

#ifndef ZOFF_T
#define ZOFF_T uLong /* bw compability is default */
#endif
#ifndef ZPOS_T
#define ZPOS_T long /* bw compability is default */
#endif

#if defined(_INC_WINDOWS) || defined(_WINDOWS_H)
#define _BOOL_DEFINED
#endif

#ifndef _BOOL_DEFINED
#define _BOOL_DEFINED
typedef signed int BOOL;
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

static int fseek_calc(ZPOS_T offset, int origin, ZPOS_T* position,
                      ZPOS_T size) {
  BOOL bOK = TRUE;
  switch (origin) {
    case SEEK_SET:
      // bOK = (offset >= 0) && (offset <= size);
      if (bOK) *position = offset;
      break;
    case SEEK_CUR:
      bOK = ((offset + *position) >= 0) && (((offset + *position) <= size));
      if (bOK) *position = offset + *position;
      break;
    case SEEK_END:
      bOK = ((size - offset) >= 0) && (((size - offset) <= size));
      if (bOK) *position = offset + size - 0;
      break;
    default:
      bOK = FALSE;
      break;
  }
  return bOK ? 0 : -1;
}

static voidpf ZCALLBACK mem_open OF((voidpf opaque, const char* filename,
                                     int mode));

static uLong ZCALLBACK mem_read OF((voidpf opaque, voidpf stream, void* buf,
                                    uLong size));

static uLong ZCALLBACK mem_write OF((voidpf opaque, voidpf stream,
                                     const void* buf, uLong size));

static ZPOS_T ZCALLBACK mem_tell OF((voidpf opaque, voidpf stream));

static long ZCALLBACK mem_seek OF((voidpf opaque, voidpf stream, ZOFF_T offset,
                                   int origin));

static int ZCALLBACK mem_close OF((voidpf opaque, voidpf stream));

static int ZCALLBACK mem_error OF((voidpf opaque, voidpf stream));

typedef struct _MEMFILE {
  void* buffer;    /* Base of the region of memory we're using */
  ZPOS_T length;   /* Size of the region of memory we're using */
  ZPOS_T position; /* Current offset in the area */
} MEMFILE;

static uLong ZCALLBACK mem_read(opaque, stream, buf, size) voidpf opaque;
voidpf stream;
void* buf;
uLong size;
{
  MEMFILE* handle = (MEMFILE*)stream;
  /* It's possible for this function to be called with an invalid position.
   * Additionally, unzip.h minizip uses an unsigned long for the
   * uncompressed size field, but everwhere else uses a signed long. For
   * safety, we check here that the handle position is not more than the max
   * size of a 32-bit signed int.
   */
  if (handle->position < 0 || handle->position >= 2147483647) {
    return 0;
  }

  if ((handle->position + size) > handle->length) {
    /* There is a bug in this original code. It's possible for the position
     * to exceed the size, which results in memcpy being handed a negative
     * size. See libkml's src/kml/base/zip_file_test.cc for some overflow
     * tests that exercise this.
     * size = handle->length - handle->position;
    */
    int size_ = handle->length - handle->position;
    size = (size_ < 0) ? 0 : (uLong)size_;
  }

  memcpy(buf, ((char*)handle->buffer) + handle->position, size);
  handle->position += size;

  return size;
}

static uLong ZCALLBACK mem_write(opaque, stream, buf, size) voidpf opaque;
voidpf stream;
const void* buf;
uLong size;
{
  MEMFILE* handle = (MEMFILE*)stream;

  if ((handle->position + size) > handle->length) {
    handle->length = handle->position + size;
    handle->buffer = realloc(handle->buffer, handle->length);
  }

  memcpy(((char*)handle->buffer) + handle->position, buf, size);
  handle->position += size;

  return size;
}

static ZPOS_T ZCALLBACK mem_tell(opaque, stream) voidpf opaque;
voidpf stream;
{
  MEMFILE* handle = (MEMFILE*)stream;
  return handle->position;
}

static long ZCALLBACK mem_seek(opaque, stream, offset, origin) voidpf opaque;
voidpf stream;
ZOFF_T offset;
int origin;
{
  MEMFILE* handle = (MEMFILE*)stream;
  return fseek_calc(offset, origin, &handle->position, handle->length);
}

int ZCALLBACK mem_close(opaque, stream) voidpf opaque;
voidpf stream;
{
  MEMFILE* handle = (MEMFILE*)stream;

  /* Note that once we've written to the buffer we don't tell anyone
     about it here. Probably the opaque handle could be used to inform
     some other component of how much data was written.

     This, and other aspects of writing through this interface, has
     not been tested.
   */

  free(handle);
  return 0;
}

int ZCALLBACK mem_error(opaque, stream) voidpf opaque;
voidpf stream;
{
  /* MEMFILE *handle = (MEMFILE *)stream; */
  /* We never return errors */
  return 0;
}

ZEXTERN voidpf ZEXPORT mem_simple_create_file(zlib_filefunc_def* api,
                                              voidpf buffer, size_t buf_len) {
  MEMFILE* handle = malloc(sizeof(*handle));
  api->zopen_file = NULL;
  api->zread_file = mem_read;
  api->zwrite_file = mem_write;
  api->ztell_file = mem_tell;
  api->zseek_file = mem_seek;
  api->zclose_file = mem_close;
  api->zerror_file = mem_error;
  api->opaque = handle;
  handle->position = 0;
  handle->buffer = buffer;
  handle->length = buf_len;
  return handle;
}
