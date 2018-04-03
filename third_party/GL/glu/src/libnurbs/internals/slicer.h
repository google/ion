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
 * SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008)
 * Copyright (C) 1991-2000 Silicon Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice including the dates of first publication and
 * either this permission notice or a reference to
 * http://oss.sgi.com/projects/FreeB/
 * shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * SILICON GRAPHICS, INC. BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of Silicon Graphics, Inc.
 * shall not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization from
 * Silicon Graphics, Inc.
 */

/*
 * slicer.h
 *
 */

#ifndef __gluslicer_h_
#define __gluslicer_h_

#include "trimregion.h"
#include "mesher.h"
#include "coveandtiler.h"
#include "primitiveStream.h"
#include "rectBlock.h"

class Backend;
class Arc;
class TrimVertex;

class Slicer : public CoveAndTiler, public Mesher {
public:
    			Slicer( Backend & );
			~Slicer( void );
    void		slice( Arc_ptr );
    void		slice_old( Arc_ptr);
    void		slice_new( Arc_ptr );
    void                evalStream(primStream* );
    void                evalRBArray(rectBlockArray* rbArray, gridWrap* grid);

    void		outline( Arc_ptr );
    void		setstriptessellation( REAL, REAL );
    void		setisolines( int );

    void                set_ulinear(int ulinear_flag)
      {
	ulinear = ulinear_flag;
      }
    void                set_vlinear(int vlinear_flag)
      {
	vlinear = vlinear_flag;
      }
private:
    Backend&		backend;
    REAL		oneOverDu;
    REAL		du, dv;
    int			isolines;

    void		outline( void );
    void		initGridlines( void );
    void		advanceGridlines( long );

    int                ulinear; //indicate whether uorder is 2 or not
    int                vlinear; //indicate whether vorder is 2 or not
};
#endif /* __gluslicer_h_ */
