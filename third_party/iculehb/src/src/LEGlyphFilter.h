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
 *
 * (C) Copyright IBM Corp. 1998-2011 - All Rights Reserved
 *
 */

#ifndef __LEGLYPHFILTER__H
#define __LEGLYPHFILTER__H

#include "LETypes.h"

U_NAMESPACE_BEGIN

#ifndef U_HIDE_INTERNAL_API
/**
 * This is a helper class that is used to
 * recognize a set of glyph indices.
 *
 * @internal
 */
class LEGlyphFilter /* not : public UObject because this is an interface/mixin class */ {
public:
    /**
     * Destructor.
     * @internal
     */
    virtual ~LEGlyphFilter();

    /**
     * This method is used to test a particular
     * glyph index to see if it is in the set
     * recognized by the filter.
     *
     * @param glyph - the glyph index to be tested
     *
     * @return TRUE if the glyph index is in the set.
     *
     * @internal
     */
    virtual le_bool accept(LEGlyphID glyph) const = 0;
};
#endif  /* U_HIDE_INTERNAL_API */

U_NAMESPACE_END
#endif
