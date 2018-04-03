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
 * Copyright © 2012,2013  Google, Inc.
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
 * Google Author(s): Behdad Esfahbod
 */

#include "hb-buffer-private.hh"


static const char *serialize_formats[] = {
  "text",
  "json",
  nullptr
};

/**
 * hb_buffer_serialize_list_formats:
 *
 * Returns a list of supported buffer serialization formats.
 *
 * Return value: (transfer none):
 * A string array of buffer serialization formats. Should not be freed.
 *
 * Since: 0.9.7
 **/
const char **
hb_buffer_serialize_list_formats (void)
{
  return serialize_formats;
}

/**
 * hb_buffer_serialize_format_from_string:
 * @str: (array length=len) (element-type uint8_t): a string to parse
 * @len: length of @str, or -1 if string is %NULL terminated
 *
 * Parses a string into an #hb_buffer_serialize_format_t. Does not check if
 * @str is a valid buffer serialization format, use
 * hb_buffer_serialize_list_formats() to get the list of supported formats.
 *
 * Return value: 
 * The parsed #hb_buffer_serialize_format_t.
 *
 * Since: 0.9.7
 **/
hb_buffer_serialize_format_t
hb_buffer_serialize_format_from_string (const char *str, int len)
{
  /* Upper-case it. */
  return (hb_buffer_serialize_format_t) (hb_tag_from_string (str, len) & ~0x20202020u);
}

/**
 * hb_buffer_serialize_format_to_string:
 * @format: an #hb_buffer_serialize_format_t to convert.
 *
 * Converts @format to the string corresponding it, or %NULL if it is not a valid
 * #hb_buffer_serialize_format_t.
 *
 * Return value: (transfer none):
 * A %NULL terminated string corresponding to @format. Should not be freed.
 *
 * Since: 0.9.7
 **/
const char *
hb_buffer_serialize_format_to_string (hb_buffer_serialize_format_t format)
{
  switch (format)
  {
    case HB_BUFFER_SERIALIZE_FORMAT_TEXT:	return serialize_formats[0];
    case HB_BUFFER_SERIALIZE_FORMAT_JSON:	return serialize_formats[1];
    default:
    case HB_BUFFER_SERIALIZE_FORMAT_INVALID:	return nullptr;
  }
}

static unsigned int
_hb_buffer_serialize_glyphs_json (hb_buffer_t *buffer,
				  unsigned int start,
				  unsigned int end,
				  char *buf,
				  unsigned int buf_size,
				  unsigned int *buf_consumed,
				  hb_font_t *font,
				  hb_buffer_serialize_flags_t flags)
{
  hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, nullptr);
  hb_glyph_position_t *pos = (flags & HB_BUFFER_SERIALIZE_FLAG_NO_POSITIONS) ?
			     nullptr : hb_buffer_get_glyph_positions (buffer, nullptr);

  *buf_consumed = 0;
  hb_position_t x = 0, y = 0;
  for (unsigned int i = start; i < end; i++)
  {
    char b[1024];
    char *p = b;

    /* In the following code, we know b is large enough that no overflow can happen. */

#define APPEND(s) HB_STMT_START { strcpy (p, s); p += strlen (s); } HB_STMT_END

    if (i)
      *p++ = ',';

    *p++ = '{';

    APPEND ("\"g\":");
    if (!(flags & HB_BUFFER_SERIALIZE_FLAG_NO_GLYPH_NAMES))
    {
      char g[128];
      hb_font_glyph_to_string (font, info[i].codepoint, g, sizeof (g));
      *p++ = '"';
      for (char *q = g; *q; q++) {
        if (*q == '"')
	  *p++ = '\\';
	*p++ = *q;
      }
      *p++ = '"';
    }
    else
      p += MAX (0, snprintf (p, ARRAY_LENGTH (b) - (p - b), "%u", info[i].codepoint));

    if (!(flags & HB_BUFFER_SERIALIZE_FLAG_NO_CLUSTERS)) {
      p += MAX (0, snprintf (p, ARRAY_LENGTH (b) - (p - b), ",\"cl\":%u", info[i].cluster));
    }

    if (!(flags & HB_BUFFER_SERIALIZE_FLAG_NO_POSITIONS))
    {
      p += MAX (0, snprintf (p, ARRAY_LENGTH (b) - (p - b), ",\"dx\":%d,\"dy\":%d",
			     x+pos[i].x_offset, y+pos[i].y_offset));
      if (!(flags & HB_BUFFER_SERIALIZE_FLAG_NO_ADVANCES))
	p += MAX (0, snprintf (p, ARRAY_LENGTH (b) - (p - b), ",\"ax\":%d,\"ay\":%d",
			       pos[i].x_advance, pos[i].y_advance));
    }

    if (flags & HB_BUFFER_SERIALIZE_FLAG_GLYPH_FLAGS)
    {
      if (info[i].mask & HB_GLYPH_FLAG_DEFINED)
	p += MAX (0, snprintf (p, ARRAY_LENGTH (b) - (p - b), ",\"fl\":%u", info[i].mask & HB_GLYPH_FLAG_DEFINED));
    }

    if (flags & HB_BUFFER_SERIALIZE_FLAG_GLYPH_EXTENTS)
    {
      hb_glyph_extents_t extents;
      hb_font_get_glyph_extents(font, info[i].codepoint, &extents);
      p += MAX (0, snprintf (p, ARRAY_LENGTH (b) - (p - b), ",\"xb\":%d,\"yb\":%d",
		extents.x_bearing, extents.y_bearing));
      p += MAX (0, snprintf (p, ARRAY_LENGTH (b) - (p - b), ",\"w\":%d,\"h\":%d",
		extents.width, extents.height));
    }

    *p++ = '}';

    unsigned int l = p - b;
    if (buf_size > l)
    {
      memcpy (buf, b, l);
      buf += l;
      buf_size -= l;
      *buf_consumed += l;
      *buf = '\0';
    } else
      return i - start;

    if (pos && (flags & HB_BUFFER_SERIALIZE_FLAG_NO_ADVANCES))
    {
      x += pos[i].x_advance;
      y += pos[i].y_advance;
    }
  }

  return end - start;
}

static unsigned int
_hb_buffer_serialize_glyphs_text (hb_buffer_t *buffer,
				  unsigned int start,
				  unsigned int end,
				  char *buf,
				  unsigned int buf_size,
				  unsigned int *buf_consumed,
				  hb_font_t *font,
				  hb_buffer_serialize_flags_t flags)
{
  hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, nullptr);
  hb_glyph_position_t *pos = (flags & HB_BUFFER_SERIALIZE_FLAG_NO_POSITIONS) ?
			     nullptr : hb_buffer_get_glyph_positions (buffer, nullptr);

  *buf_consumed = 0;
  hb_position_t x = 0, y = 0;
  for (unsigned int i = start; i < end; i++)
  {
    char b[1024];
    char *p = b;

    /* In the following code, we know b is large enough that no overflow can happen. */

    if (i)
      *p++ = '|';

    if (!(flags & HB_BUFFER_SERIALIZE_FLAG_NO_GLYPH_NAMES))
    {
      hb_font_glyph_to_string (font, info[i].codepoint, p, 128);
      p += strlen (p);
    }
    else
      p += MAX (0, snprintf (p, ARRAY_LENGTH (b) - (p - b), "%u", info[i].codepoint));

    if (!(flags & HB_BUFFER_SERIALIZE_FLAG_NO_CLUSTERS)) {
      p += MAX (0, snprintf (p, ARRAY_LENGTH (b) - (p - b), "=%u", info[i].cluster));
    }

    if (!(flags & HB_BUFFER_SERIALIZE_FLAG_NO_POSITIONS))
    {
      if (x+pos[i].x_offset || y+pos[i].y_offset)
	p += MAX (0, snprintf (p, ARRAY_LENGTH (b) - (p - b), "@%d,%d", x+pos[i].x_offset, y+pos[i].y_offset));

      if (!(flags & HB_BUFFER_SERIALIZE_FLAG_NO_ADVANCES))
      {
	*p++ = '+';
	p += MAX (0, snprintf (p, ARRAY_LENGTH (b) - (p - b), "%d", pos[i].x_advance));
	if (pos[i].y_advance)
	  p += MAX (0, snprintf (p, ARRAY_LENGTH (b) - (p - b), ",%d", pos[i].y_advance));
      }
    }

    if (flags & HB_BUFFER_SERIALIZE_FLAG_GLYPH_FLAGS)
    {
      if (info[i].mask &HB_GLYPH_FLAG_DEFINED)
	p += MAX (0, snprintf (p, ARRAY_LENGTH (b) - (p - b), "#%X", info[i].mask &HB_GLYPH_FLAG_DEFINED));
    }

    if (flags & HB_BUFFER_SERIALIZE_FLAG_GLYPH_EXTENTS)
    {
      hb_glyph_extents_t extents;
      hb_font_get_glyph_extents(font, info[i].codepoint, &extents);
      p += MAX (0, snprintf (p, ARRAY_LENGTH (b) - (p - b), "<%d,%d,%d,%d>", extents.x_bearing, extents.y_bearing, extents.width, extents.height));
    }

    unsigned int l = p - b;
    if (buf_size > l)
    {
      memcpy (buf, b, l);
      buf += l;
      buf_size -= l;
      *buf_consumed += l;
      *buf = '\0';
    } else
      return i - start;

    if (pos && (flags & HB_BUFFER_SERIALIZE_FLAG_NO_ADVANCES))
    {
      x += pos[i].x_advance;
      y += pos[i].y_advance;
    }
  }

  return end - start;
}


			      const char **end_ptr, /* May be nullptr */
			      hb_font_t *font, /* May be nullptr */
			      hb_buffer_serialize_format_t format)
{
  const char *end;
  if (!end_ptr)
    end_ptr = &end;
  *end_ptr = buf;

  assert ((!buffer->len && buffer->content_type == HB_BUFFER_CONTENT_TYPE_INVALID) ||
	  buffer->content_type == HB_BUFFER_CONTENT_TYPE_GLYPHS);

  if (buf_len == -1)
    buf_len = strlen (buf);

  if (!buf_len)
  {
    *end_ptr = buf;
    return false;
  }

  hb_buffer_set_content_type (buffer, HB_BUFFER_CONTENT_TYPE_GLYPHS);

  if (!font)
    font = hb_font_get_empty ();

  switch (format)
  {
    case HB_BUFFER_SERIALIZE_FORMAT_TEXT:
      return _hb_buffer_deserialize_glyphs_text (buffer,
						 buf, buf_len, end_ptr,
						 font);

    case HB_BUFFER_SERIALIZE_FORMAT_JSON:
      return _hb_buffer_deserialize_glyphs_json (buffer,
						 buf, buf_len, end_ptr,
						 font);

    default:
    case HB_BUFFER_SERIALIZE_FORMAT_INVALID:
      return false;

  }
}
