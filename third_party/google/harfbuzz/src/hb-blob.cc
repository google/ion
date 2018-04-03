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
 * Copyright © 2009  Red Hat, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 */

/* http://www.oracle.com/technetwork/articles/servers-storage-dev/standardheaderfiles-453865.html */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "hb-private.hh"
#include "hb-debug.hh"

#include "hb-object-private.hh"

#ifdef HAVE_SYS_MMAN_H
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <sys/mman.h>
#endif /* HAVE_SYS_MMAN_H */

#include <stdio.h>
#include <errno.h>


struct hb_blob_t {
  hb_object_header_t header;
  ASSERT_POD ();

  bool immutable;

  const char *data;
  unsigned int length;
  hb_memory_mode_t mode;

  void *user_data;
  hb_destroy_func_t destroy;
};


static bool _try_writable (hb_blob_t *blob);

static void
_hb_blob_destroy_user_data (hb_blob_t *blob)
{
  if (blob->destroy) {
    blob->destroy (blob->user_data);
    blob->user_data = nullptr;
    blob->destroy = nullptr;
  }
}



    nullptr, /* data */
    0, /* length */
    HB_MEMORY_MODE_READONLY, /* mode */

    nullptr, /* user_data */
    nullptr  /* destroy */
  };

  return const_cast<hb_blob_t *> (&_hb_blob_nil);
}


  blob->mode = HB_MEMORY_MODE_READONLY;
  return false;
}

static bool
_try_writable (hb_blob_t *blob)
{
  if (blob->immutable)
    return false;

  if (blob->mode == HB_MEMORY_MODE_WRITABLE)
    return true;

  if (blob->mode == HB_MEMORY_MODE_READONLY_MAY_MAKE_WRITABLE && _try_writable_inplace (blob))
    return true;

  if (blob->mode == HB_MEMORY_MODE_WRITABLE)
    return true;


  DEBUG_MSG_FUNC (BLOB, blob, "current data is -> %p\n", blob->data);

  char *new_data;

  new_data = (char *) malloc (blob->length);
  if (unlikely (!new_data))
    return false;

  DEBUG_MSG_FUNC (BLOB, blob, "dupped successfully -> %p\n", blob->data);

  memcpy (new_data, blob->data, blob->length);
  _hb_blob_destroy_user_data (blob);
  blob->mode = HB_MEMORY_MODE_WRITABLE;
  blob->data = new_data;
  blob->user_data = new_data;
  blob->destroy = free;

  return true;
}
