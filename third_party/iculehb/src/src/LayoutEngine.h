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
 * (C) Copyright IBM Corp. and others 1998-2013 - All Rights Reserved
 * (C) Copyright Google, Inc. 2012-2013 - All Rights Reserved
 */

#ifndef __LAYOUTENGINE_H
#define __LAYOUTENGINE_H

#include "LETypes.h"

struct hb_font_t;
struct hb_buffer_t;

U_NAMESPACE_BEGIN

class LEFontInstance;
class LEGlyphStorage;

/**
 * This is a class used to do complex text layout. The text must all
 * be in a single font, script, and language. An instance of a LayoutEngine can be
 * created by calling the layoutEngineFactory method. Fonts are identified by
 * instances of the LEFontInstance class. Script and language codes are identified
 * by integer codes, which are defined in ScriptAndLanuageTags.h.
 *
 * Note that this class is not public API. It is declared public so that it can be
 * exported from the library that it is a part of.
 *
 * The input to the layout process is an array of characters in logical order,
 * and a starting X, Y position for the text. The output is an array of glyph indices,
 * an array of character indices for the glyphs, and an array of glyph positions.
 * These arrays are protected members of LayoutEngine which can be retreived by a
 * public method. The reset method can be called to free these arrays so that the
 * LayoutEngine can be reused.
 *
 * @see LEFontInstance
 * @see ScriptAndLanguageTags.h
 *
 * @stable ICU 2.8
 */
class U_LAYOUT_API LayoutEngine : public UObject {
public:
#ifndef U_HIDE_INTERNAL_API
    /** @internal Flag to request kerning. */
    static const le_int32 kTypoFlagKern;
    /** @internal Flag to request ligatures. */
    static const le_int32 kTypoFlagLiga;
    /** @internal Flag to request layout along vertical baseline. */
    static const le_int32 kTypoFlagVbas;
#endif  /* U_HIDE_INTERNAL_API */

private:

   hb_font_t *fHbFont;
   hb_buffer_t *fHbBuffer;
   LEGlyphStorage *fGlyphStorage;
   le_int32 fTypoFlags;

#ifndef U_HIDE_INTERNAL_API
    /**
     * This constructs an instance for a given font, script and language. Subclass constructors
     * must call this constructor.
     *
     * @param fontInstance - the font for the text
     * @param scriptCode - the script for the text
     * @param languageCode - the language for the text
     * @param typoFlags - the typographic control flags for the text (a bitfield).  Use kTypoFlagKern
     * if kerning is desired, kTypoFlagLiga if ligature formation is desired.  Others are reserved.
     * @param success - set to an error code if the operation fails
     *
     * @see LEFontInstance
     * @see ScriptAndLanguageTags.h
     *
     * @internal
     */
    LayoutEngine(const LEFontInstance *fontInstance,
                 le_int32 scriptCode,
                 le_int32 languageCode,
                 le_int32 typoFlags,
                 LEErrorCode &success);
#endif  /* U_HIDE_INTERNAL_API */

    // Do not enclose the protected default constructor with #ifndef U_HIDE_INTERNAL_API
    // or else the compiler will create a public default constructor.
    /**
     * This overrides the default no argument constructor to make it
     * difficult for clients to call it. Clients are expected to call
     * layoutEngineFactory.
     *
     * @internal
     */
    LayoutEngine();

public:
    /**
     * The destructor. It will free any storage allocated for the
     * glyph, character index and position arrays by calling the reset
     * method.
     *
     * @stable ICU 2.8
     */
    ~LayoutEngine();

    /**
     * This method will invoke the layout steps in their correct order by calling
     * the computeGlyphs, positionGlyphs and adjustGlyphPosition methods. It will
     * compute the glyph, character index and position arrays.
     *
     * @param chars - the input character context
     * @param offset - the offset of the first character to process
     * @param count - the number of characters to process
     * @param max - the number of characters in the input context
     * @param rightToLeft - TRUE if the characers are in a right to left directional run
     * @param x - the initial X position
     * @param y - the initial Y position
     * @param success - output parameter set to an error code if the operation fails
     *
     * @return the number of glyphs in the glyph array
     *
     * Note: The glyph, character index and position array can be accessed
     * using the getter methods below.
     *
     * Note: If you call this method more than once, you must call the reset()
     * method first to free the glyph, character index and position arrays
     * allocated by the previous call.
     *
     * @stable ICU 2.8
     */
    le_int32 layoutChars(const LEUnicode chars[], le_int32 offset, le_int32 count, le_int32 max, le_bool rightToLeft, float x, float y, LEErrorCode &success);

    /**
     * This method returns the number of glyphs in the glyph array. Note
     * that the number of glyphs will be greater than or equal to the number
     * of characters used to create the LayoutEngine.
     *
     * @return the number of glyphs in the glyph array
     *
     * @stable ICU 2.8
     */
    le_int32 getGlyphCount() const;

    /**
     * This method copies the glyph array into a caller supplied array.
     * The caller must ensure that the array is large enough to hold all
     * the glyphs.
     *
     * @param glyphs - the destiniation glyph array
     * @param success - set to an error code if the operation fails
     *
     * @stable ICU 2.8
     */
    void getGlyphs(LEGlyphID glyphs[], LEErrorCode &success) const;

    /**
     * This method copies the glyph array into a caller supplied array,
     * ORing in extra bits. (This functionality is needed by the JDK,
     * which uses 32 bits pre glyph idex, with the high 16 bits encoding
     * the composite font slot number)
     *
     * @param glyphs - the destination (32 bit) glyph array
     * @param extraBits - this value will be ORed with each glyph index
     * @param success - set to an error code if the operation fails
     *
     * @stable ICU 2.8
     */
    void getGlyphs(le_uint32 glyphs[], le_uint32 extraBits, LEErrorCode &success) const;

    /**
     * This method copies the character index array into a caller supplied array.
     * The caller must ensure that the array is large enough to hold a
     * character index for each glyph.
     *
     * @param charIndices - the destiniation character index array
     * @param success - set to an error code if the operation fails
     *
     * @stable ICU 2.8
     */
    void getCharIndices(le_int32 charIndices[], LEErrorCode &success) const;

    /**
     * This method copies the character index array into a caller supplied array.
     * The caller must ensure that the array is large enough to hold a
     * character index for each glyph.
     *
     * @param charIndices - the destiniation character index array
     * @param indexBase - an offset which will be added to each index
     * @param success - set to an error code if the operation fails
     *
     * @stable ICU 2.8
     */
    void getCharIndices(le_int32 charIndices[], le_int32 indexBase, LEErrorCode &success) const;

    /**
     * This method copies the position array into a caller supplied array.
     * The caller must ensure that the array is large enough to hold an
     * X and Y position for each glyph, plus an extra X and Y for the
     * advance of the last glyph.
     *
     * @param positions - the destiniation position array
     * @param success - set to an error code if the operation fails
     *
     * @stable ICU 2.8
     */
    void getGlyphPositions(float positions[], LEErrorCode &success) const;

    /**
     * This method returns the X and Y position of the glyph at
     * the given index.
     *
     * Input parameters:
     * @param glyphIndex - the index of the glyph
     *
     * Output parameters:
     * @param x - the glyph's X position
     * @param y - the glyph's Y position
     * @param success - set to an error code if the operation fails
     *
     * @stable ICU 2.8
     */
    void getGlyphPosition(le_int32 glyphIndex, float &x, float &y, LEErrorCode &success) const;

    /**
     * This method frees the glyph, character index and position arrays
     * so that the LayoutEngine can be reused to layout a different
     * characer array. (This method is also called by the destructor)
     *
     * @stable ICU 2.8
     */
    void reset();

    /**
     * This method returns a LayoutEngine capable of laying out text
     * in the given font, script and langauge. Note that the LayoutEngine
     * returned may be a subclass of LayoutEngine.
     *
     * @param fontInstance - the font of the text
     * @param scriptCode - the script of the text
     * @param languageCode - the language of the text
     * @param success - output parameter set to an error code if the operation fails
     *
     * @return a LayoutEngine which can layout text in the given font.
     *
     * @see LEFontInstance
     *
     * @stable ICU 2.8
     */
    static LayoutEngine *layoutEngineFactory(const LEFontInstance *fontInstance, le_int32 scriptCode, le_int32 languageCode, LEErrorCode &success);

    /**
     * Override of existing call that provides flags to control typography.
     * @stable ICU 3.4
     */
    static LayoutEngine *layoutEngineFactory(const LEFontInstance *fontInstance, le_int32 scriptCode, le_int32 languageCode, le_int32 typo_flags, LEErrorCode &success);

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @stable ICU 2.8
     */
    virtual UClassID getDynamicClassID() const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @stable ICU 2.8
     */
    static UClassID getStaticClassID();

};

U_NAMESPACE_END
#endif
