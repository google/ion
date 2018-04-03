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
 * Copyright © 2018  Ebrahim Byagowi
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
 */

#ifndef HB_OT_COLOR_SBIX_TABLE_HH
#define HB_OT_COLOR_SBIX_TABLE_HH

#include "hb-open-type-private.hh"

#define HB_OT_TAG_sbix HB_TAG('s','b','i','x')

namespace OT {


struct SBIXGlyph
{
  HBINT16	xOffset;	/* The horizontal (x-axis) offset from the left
				 * edge of the graphic to the glyph’s origin.
				 * That is, the x-coordinate of the point on the
				 * baseline at the left edge of the glyph. */
  HBINT16	yOffset;	/* The vertical (y-axis) offset from the bottom
				 * edge of the graphic to the glyph’s origin.
				 * That is, the y-coordinate of the point on the
				 * baseline at the left edge of the glyph. */
  Tag		graphicType;	/* Indicates the format of the embedded graphic
				 * data: one of 'jpg ', 'png ' or 'tiff', or the
				 * special format 'dupe'. */
  HBUINT8	data[VAR];	/* The actual embedded graphic data. The total
				 * length is inferred from sequential entries in
				 * the glyphDataOffsets array and the fixed size
				 * (8 bytes) of the preceding fields. */
  public:
  DEFINE_SIZE_ARRAY (8, data);
};

struct SBIXStrike
{
  friend struct sbix;

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  c->check_array (imageOffsetsZ,
				  sizeof (HBUINT32),
				  1 + c->num_glyphs));
  }

  HBUINT16		ppem;		/* The PPEM size for which this strike was designed. */
  HBUINT16		resolution;	/* The device pixel density (in PPI) for which this
					 * strike was designed. (E.g., 96 PPI, 192 PPI.) */
  protected:
  LOffsetTo<SBIXGlyph>	imageOffsetsZ[VAR]; // VAR=maxp.numGlyphs + 1
					/* Offset from the beginning of the strike data header
					 * to bitmap data for an individual glyph ID. */
  public:
  DEFINE_SIZE_STATIC (8);
};

/*
 * sbix -- Standard Bitmap Graphics Table
 * https://docs.microsoft.com/en-us/typography/opentype/spec/sbix
 */

struct sbix
{
  static const hb_tag_t tableTag = HB_OT_TAG_sbix;

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && strikes.sanitize (c, this));
  }

  struct accelerator_t
  {
    inline void init (hb_face_t *face)
    {
      num_glyphs = hb_face_get_glyph_count (face);

      OT::Sanitizer<OT::sbix> sanitizer;
      sanitizer.set_num_glyphs (num_glyphs);
      sbix_blob = sanitizer.sanitize (face->reference_table (HB_OT_TAG_sbix));
      sbix_len = hb_blob_get_length (sbix_blob);
      sbix_table = OT::Sanitizer<OT::sbix>::lock_instance (sbix_blob);

    }

    inline void fini (void)
    {
      hb_blob_destroy (sbix_blob);
    }

    inline void dump (void (*callback) (const uint8_t* data, unsigned int length,
        unsigned int group, unsigned int gid)) const
    {
      for (unsigned group = 0; group < sbix_table->strikes.len; ++group)
      {
        const SBIXStrike &strike = sbix_table->strikes[group](sbix_table);
        for (unsigned int glyph = 0; glyph < num_glyphs; ++glyph)
          if (strike.imageOffsetsZ[glyph + 1] - strike.imageOffsetsZ[glyph] > 0)
          {
            const SBIXGlyph &sbixGlyph = strike.imageOffsetsZ[glyph]((const void *) &strike);
            callback ((const uint8_t*) sbixGlyph.data,
              strike.imageOffsetsZ[glyph + 1] - strike.imageOffsetsZ[glyph] - 8,
              group, glyph);
          }
      }
    }

    private:
    hb_blob_t *sbix_blob;
    const sbix *sbix_table;

    unsigned int sbix_len;
    unsigned int num_glyphs;

  };

  protected:
  HBUINT16	version;	/* Table version number — set to 1 */
  HBUINT16	flags;		/* Bit 0: Set to 1. Bit 1: Draw outlines.
				 * Bits 2 to 15: reserved (set to 0). */
  ArrayOf<LOffsetTo<SBIXStrike>, HBUINT32>
		strikes;	/* Offsets from the beginning of the 'sbix'
				 * table to data for each individual bitmap strike. */
  public:
  DEFINE_SIZE_ARRAY (8, strikes);
};

} /* namespace OT */

#endif /* HB_OT_COLOR_SBIX_TABLE_HH */
