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
*/

#ifndef _SAMPLECOMPBOT_H
#define _SAMPLECOMPBOT_H

#include "sampleMonoPoly.h"

void findBotLeftSegment(vertexArray* leftChain, 
			Int leftEnd,
			Int leftCorner,
			Real u,
			Int& ret_index_mono,
			Int& ret_index_pass);

void findBotRightSegment(vertexArray* rightChain, 
			 Int rightEnd,
			 Int rightCorner,
			 Real u,
			 Int& ret_index_mono,
			 Int& ret_index_pass);


void sampleBotRightWithGridLinePost(Real* botVertex,
				    vertexArray* rightChain,
				    Int rightEnd,
				    Int segIndexMono,
				    Int segIndexPass,
				    Int rightCorner,
				    gridWrap* grid,
				    Int gridV,
				    Int leftU,
				    Int rightU,
				    primStream* pStream);


void sampleBotRightWithGridLine(Real* botVertex, 
				vertexArray* rightChain,
				Int rightEnd,
				Int rightCorner,
				gridWrap* grid,
				Int gridV,
				Int leftU,
				Int rightU,
				primStream* pStream);


void sampleBotLeftWithGridLinePost(Real* botVertex,
				   vertexArray* leftChain,
				   Int leftEnd,
				   Int segIndexMono,
				   Int segIndexPass,
				   Int leftCorner,
				   gridWrap* grid,
				   Int gridV,
				   Int leftU, 
				   Int rightU,
				   primStream* pStream);


void sampleBotLeftWithGridLine(Real* botVertex,
			       vertexArray* leftChain,
			       Int leftEnd,
			       Int leftCorner,
			       gridWrap* grid,
			       Int gridV,
			       Int leftU,
			       Int rightU,
			       primStream* pStream);


Int findBotSeparator(vertexArray* leftChain,
		     Int leftEnd,
		     Int leftCorner,
		     vertexArray* rightChain,
		     Int rightEnd,
		     Int rightCorner,
		     Int& ret_sep_left,
		     Int& ret_sep_right);

void sampleCompBot(Real* botVertex,
		   vertexArray* leftChain,
		   Int leftEnd,
		   vertexArray* rightChain,
		   Int rightEnd,
		   gridBoundaryChain* leftGridChain,
		   gridBoundaryChain* rightGridChain,
		   Int gridIndex,
		   Int down_leftCornerWhere,
		   Int down_leftCornerIndex,
		   Int down_rightCornerWhere,
		   Int down_rightCornerIndex,
		   primStream* pStream);

void sampleCompBotSimple(Real* botVertex,
		   vertexArray* leftChain,
		   Int leftEnd,
		   vertexArray* rightChain,
		   Int rightEnd,
		   gridBoundaryChain* leftGridChain,
		   gridBoundaryChain* rightGridChain,
		   Int gridIndex,
		   Int down_leftCornerWhere,
		   Int down_leftCornerIndex,
		   Int down_rightCornerWhere,
		   Int down_rightCornerIndex,
		   primStream* pStream);

#endif
