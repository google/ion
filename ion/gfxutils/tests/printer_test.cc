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

#include "ion/gfxutils/printer.h"

#include <sstream>
#include <string>
#include <vector>

#include "ion/base/datacontainer.h"
#include "ion/base/logchecker.h"
#include "ion/base/serialize.h"
#include "ion/base/stringutils.h"
#include "ion/base/tests/multilinestringsequal.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/cubemaptexture.h"
#include "ion/gfx/image.h"
#include "ion/gfx/indexbuffer.h"
#include "ion/gfx/node.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/texture.h"
#include "ion/gfxutils/buffertoattributebinder.h"
#include "ion/math/range.h"
#include "ion/math/vector.h"
#include "ion/port/nullptr.h"  // For kNullFunction.
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfxutils {

namespace {

using gfx::Attribute;
using gfx::AttributeArray;
using gfx::AttributeArrayPtr;
using gfx::BufferObject;
using gfx::BufferObjectPtr;
using gfx::CubeMapTexture;
using gfx::CubeMapTexturePtr;
using gfx::Image;
using gfx::ImagePtr;
using gfx::IndexBuffer;
using gfx::IndexBufferPtr;
using gfx::Node;
using gfx::NodePtr;
using gfx::Sampler;
using gfx::SamplerPtr;
using gfx::Shader;
using gfx::ShaderPtr;
using gfx::ShaderInputRegistry;
using gfx::ShaderInputRegistryPtr;
using gfx::ShaderProgram;
using gfx::ShaderProgramPtr;
using gfx::Shape;
using gfx::ShapePtr;
using gfx::StateTable;
using gfx::StateTablePtr;
using gfx::Texture;
using gfx::TexturePtr;
using gfx::Uniform;
using gfx::UniformBlock;
using gfx::UniformBlockPtr;
using math::Matrix2f;
using math::Matrix3f;
using math::Matrix4f;
using math::Point2i;
using math::Range1f;
using math::Range1i;
using math::Range2i;
using math::Vector2f;
using math::Vector2i;
using math::Vector2ui;
using math::Vector3f;
using math::Vector3i;
using math::Vector3ui;
using math::Vector4f;
using math::Vector4i;
using math::Vector4ui;

//-----------------------------------------------------------------------------
//
// Scene-building helper functions.
//
//-----------------------------------------------------------------------------

// The number of indices for index buffers.
static const int kNumIndices = 24;

// A string that is the output text when a TestScene is printed.
static const char kTestSceneText[] =
    "ION Node \"Root\" {\n"
    "  Enabled: true\n"
    "  Shader ID: \"Dummy Shader\"\n"
    "  ION StateTable {\n"
    "    Blend: true\n"
    "    CullFace: true\n"
    "    DebugOutputSynchronous: true\n"
    "    DepthTest: true\n"
    "    Dither: false\n"
    "    Multisample: false\n"
    "    PolygonOffsetFill: true\n"
    "    SampleAlphaToCoverage: true\n"
    "    SampleCoverage: false\n"
    "    ScissorTest: true\n"
    "    StencilTest: false\n"
    "    Blend Color: V[0.1, 0.2, 0.3, 0.4]\n"
    "    Blend Equations: RGB=Subtract, Alpha=ReverseSubtract\n"
    "    Blend Functions: RGB-src=SrcColor, RGB-dest=OneMinusDstColor, "
    "Alpha-src=OneMinusConstantAlpha, Alpha-dest=DstColor\n"
    "    Clear Color: V[0.4, 0.5, 0.6, 0.7]\n"
    "    Color Write Masks: R=false, G=true, B=true, A=false\n"
    "    Cull Face Mode: CullFrontAndBack\n"
    "    Front Face Mode: Clockwise\n"
    "    Clear Depth Value: 0.2\n"
    "    Depth Function: DepthNotEqual\n"
    "    Depth Range: R[0.2, 0.6]\n"
    "    Depth Write Mask: false\n"
    "    Generate Mipmap Hint: HintNicest\n"
    "    Line Width: 0.4\n"
    "    Polygon Offset: Factor=0.5, Units=2\n"
    "    Sample Coverage: Value=0.4, Inverted=true\n"
    "    Scissor Box: R[P[10, 20], P[210, 320]]\n"
    "    Stencil Functions: FFunc=StencilNever, FRef=10, FMask=0x40404040, "
    "BFunc=StencilLess, BRef=5, BMask=0x12345678\n"
    "    Stencil Operations: FFail=StencilDecrement, "
    "FDFail=StencilDecrementAndWrap, FPass=StencilIncrement, "
    "BFail=StencilIncrementAndWrap, BDFail=StencilInvert, BPass"
    "=StencilReplace\n"
    "    Clear Stencil Value: 152\n"
    "    Stencil Write Masks: F=0x12345678, B=0xbeefface\n"
    "    Viewport: R[P[10, 20], P[210, 320]]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uFV3\"\n"
    "    Type: FloatVector3\n"
    "    Value: V[4, 5, 6]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uFV4\"\n"
    "    Type: FloatVector4\n"
    "    Value: V[7, 8, 9, 10]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uIV2\"\n"
    "    Type: IntVector2\n"
    "    Value: V[2, 3]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uIV3\"\n"
    "    Type: IntVector3\n"
    "    Value: V[4, 5, 6]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uIV4\"\n"
    "    Type: IntVector4\n"
    "    Value: V[7, 8, 9, 10]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uUV2\"\n"
    "    Type: UnsignedIntVector2\n"
    "    Value: V[2, 3]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uUV3\"\n"
    "    Type: UnsignedIntVector3\n"
    "    Value: V[4, 5, 6]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uUV4\"\n"
    "    Type: UnsignedIntVector4\n"
    "    Value: V[7, 8, 9, 10]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uMat2\"\n"
    "    Type: Matrix2x2\n"
    "    Value: [[1, 2]\n"
    "            [3, 4]]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uMat3\"\n"
    "    Type: Matrix3x3\n"
    "    Value: [[1, 2, 3]\n"
    "            [4, 5, 6]\n"
    "            [7, 8, 9]]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uMat4\"\n"
    "    Type: Matrix4x4\n"
    "    Value: [[1, 2, 3, 4]\n"
    "            [5, 6, 7, 8]\n"
    "            [9, 1, 2, 3]\n"
    "            [4, 5, 6, 7]]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uFV3\"\n"
    "    Type: FloatVector3\n"
    "    Value 0: V[1, 2, 3]\n"
    "    Value 1: V[4, 5, 6]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uFV4\"\n"
    "    Type: FloatVector4\n"
    "    Value 0: V[1, 2, 3, 4]\n"
    "    Value 1: V[5, 6, 7, 8]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uIV2\"\n"
    "    Type: IntVector2\n"
    "    Value 0: V[1, 2]\n"
    "    Value 1: V[3, 4]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uIV3\"\n"
    "    Type: IntVector3\n"
    "    Value 0: V[1, 2, 3]\n"
    "    Value 1: V[4, 5, 6]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uIV4\"\n"
    "    Type: IntVector4\n"
    "    Value 0: V[1, 2, 3, 4]\n"
    "    Value 1: V[5, 6, 7, 8]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uUV2\"\n"
    "    Type: UnsignedIntVector2\n"
    "    Value 0: V[1, 2]\n"
    "    Value 1: V[3, 4]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uUV3\"\n"
    "    Type: UnsignedIntVector3\n"
    "    Value 0: V[1, 2, 3]\n"
    "    Value 1: V[4, 5, 6]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uUV4\"\n"
    "    Type: UnsignedIntVector4\n"
    "    Value 0: V[1, 2, 3, 4]\n"
    "    Value 1: V[5, 6, 7, 8]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uMat2\"\n"
    "    Type: Matrix2x2\n"
    "    Value 0: [[1, 0]\n"
    "              [0, 1]]\n"
    "    Value 1: [[2, 0]\n"
    "              [0, 2]]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uMat3\"\n"
    "    Type: Matrix3x3\n"
    "    Value 0: [[1, 0, 0]\n"
    "              [0, 1, 0]\n"
    "              [0, 0, 1]]\n"
    "    Value 1: [[2, 0, 0]\n"
    "              [0, 2, 0]\n"
    "              [0, 0, 2]]\n"
    "  }\n"
    "  ION Uniform {\n"
    "    Name: \"uMat4\"\n"
    "    Type: Matrix4x4\n"
    "    Value 0: [[1, 0, 0, 0]\n"
    "              [0, 1, 0, 0]\n"
    "              [0, 0, 1, 0]\n"
    "              [0, 0, 0, 1]]\n"
    "    Value 1: [[2, 0, 0, 0]\n"
    "              [0, 2, 0, 0]\n"
    "              [0, 0, 2, 0]\n"
    "              [0, 0, 0, 2]]\n"
    "  }\n"
    "  ION UniformBlock \"Block 1\" {\n"
    "    Enabled: true\n"
    "    ION Uniform {\n"
    "      Name: \"uInt\"\n"
    "      Type: Int\n"
    "      Value: 13\n"
    "    }\n"
    "    ION Uniform {\n"
    "      Name: \"uFloat\"\n"
    "      Type: Float\n"
    "      Value: 1.5\n"
    "    }\n"
    "    ION Uniform {\n"
    "      Name: \"uUint\"\n"
    "      Type: UnsignedInt\n"
    "      Value: 15\n"
    "    }\n"
    "    ION Uniform {\n"
    "      Name: \"uInt\"\n"
    "      Type: Int\n"
    "      Value 0: 1\n"
    "      Value 1: 2\n"
    "    }\n"
    "    ION Uniform {\n"
    "      Name: \"uFloat\"\n"
    "      Type: Float\n"
    "      Value 0: 1\n"
    "      Value 1: 2\n"
    "    }\n"
    "    ION Uniform {\n"
    "      Name: \"uUint\"\n"
    "      Type: UnsignedInt\n"
    "      Value 0: 3\n"
    "      Value 1: 4\n"
    "    }\n"
    "  }\n"
    "  ION UniformBlock {\n"
    "    Enabled: true\n"
    "    ION Uniform {\n"
    "      Name: \"uCubeTex\"\n"
    "      Type: CubeMapTexture\n"
    "      Value: ION CubeMapTexture \"Cubemap\" {\n"
    "        Image: Face=Negative X, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Negative Y, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Negative Z, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Positive X, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Positive Y, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Positive Z, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Level range: R[10, 100]\n"
    "        Multisampling: Samples=0, Fixed sample locations=true\n"
    "        Swizzles: R=Alpha, G=Blue, B=Green, A=Red\n"
    "        Sampler: ION Sampler \"Cubemap Sampler\" {\n"
    "          Autogenerating mipmaps: false\n"
    "          Texture compare mode: CompareToTexture\n"
    "          Texture compare function: Never\n"
    "          MinFilter mode: LinearMipmapLinear\n"
    "          MagFilter mode: Nearest\n"
    "          Level-of-detail range: R[-1.5, 1.5]\n"
    "          Wrap modes: R=ClampToEdge, S=MirroredRepeat, T=ClampToEdge\n"
    "        }\n"
    "      }\n"
    "    }\n"
    "    ION Uniform {\n"
    "      Name: \"uTex\"\n"
    "      Type: Texture\n"
    "      Value: ION Texture \"Texture\" {\n"
    "        Image: Face=None, Format=Rgb888, Width=2, Height=2, Depth=1, "
    "Type=Dense, Dimensions=2\n"
    "        Level range: R[0, 1000]\n"
    "        Multisampling: Samples=0, Fixed sample locations=true\n"
    "        Swizzles: R=Red, G=Green, B=Blue, A=Alpha\n"
    "        Sampler: ION Sampler \"Sampler\" {\n"
    "          Autogenerating mipmaps: false\n"
    "          Texture compare mode: None\n"
    "          Texture compare function: Less\n"
    "          MinFilter mode: Nearest\n"
    "          MagFilter mode: Nearest\n"
    "          Level-of-detail range: R[-1000, 1000]\n"
    "          Wrap modes: R=Repeat, S=Repeat, T=Repeat\n"
    "        }\n"
    "      }\n"
    "    }\n"
    "    ION Uniform {\n"
    "      Name: \"uFV2\"\n"
    "      Type: FloatVector2\n"
    "      Value: V[2, 3]\n"
    "    }\n"

    "    ION Uniform {\n"
    "      Name: \"uCubeTex\"\n"
    "      Type: CubeMapTexture\n"
    "      Value 0: ION CubeMapTexture \"Cubemap\" {\n"
    "        Image: Face=Negative X, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Negative Y, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Negative Z, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Positive X, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Positive Y, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Positive Z, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Level range: R[10, 100]\n"
    "        Multisampling: Samples=0, Fixed sample locations=true\n"
    "        Swizzles: R=Alpha, G=Blue, B=Green, A=Red\n"
    "        Sampler: ION Sampler \"Cubemap Sampler\" {\n"
    "          Autogenerating mipmaps: false\n"
    "          Texture compare mode: CompareToTexture\n"
    "          Texture compare function: Never\n"
    "          MinFilter mode: LinearMipmapLinear\n"
    "          MagFilter mode: Nearest\n"
    "          Level-of-detail range: R[-1.5, 1.5]\n"
    "          Wrap modes: R=ClampToEdge, S=MirroredRepeat, T=ClampToEdge\n"
    "        }\n"
    "      }\n"
    "      Value 1: ION CubeMapTexture \"Cubemap\" {\n"
    "        Image: Face=Negative X, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Negative Y, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Negative Z, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Positive X, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Positive Y, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Image: Face=Positive Z, Format=Rgb888, Width=2, Height=2, "
    "Depth=1, Type=Dense, Dimensions=2\n"
    "        Level range: R[10, 100]\n"
    "        Multisampling: Samples=0, Fixed sample locations=true\n"
    "        Swizzles: R=Alpha, G=Blue, B=Green, A=Red\n"
    "        Sampler: ION Sampler \"Cubemap Sampler\" {\n"
    "          Autogenerating mipmaps: false\n"
    "          Texture compare mode: CompareToTexture\n"
    "          Texture compare function: Never\n"
    "          MinFilter mode: LinearMipmapLinear\n"
    "          MagFilter mode: Nearest\n"
    "          Level-of-detail range: R[-1.5, 1.5]\n"
    "          Wrap modes: R=ClampToEdge, S=MirroredRepeat, T=ClampToEdge\n"
    "        }\n"
    "      }\n"
    "    }\n"
    "    ION Uniform {\n"
    "      Name: \"uTex\"\n"
    "      Type: Texture\n"
    "      Value 0: ION Texture \"Texture\" {\n"
    "        Image: Face=None, Format=Rgb888, Width=2, Height=2, Depth=1, "
    "Type=Dense, Dimensions=2\n"
    "        Level range: R[0, 1000]\n"
    "        Multisampling: Samples=0, Fixed sample locations=true\n"
    "        Swizzles: R=Red, G=Green, B=Blue, A=Alpha\n"
    "        Sampler: ION Sampler \"Sampler\" {\n"
    "          Autogenerating mipmaps: false\n"
    "          Texture compare mode: None\n"
    "          Texture compare function: Less\n"
    "          MinFilter mode: Nearest\n"
    "          MagFilter mode: Nearest\n"
    "          Level-of-detail range: R[-1000, 1000]\n"
    "          Wrap modes: R=Repeat, S=Repeat, T=Repeat\n"
    "        }\n"
    "      }\n"
    "      Value 1: ION Texture \"Texture\" {\n"
    "        Image: Face=None, Format=Rgb888, Width=2, Height=2, Depth=1, "
    "Type=Dense, Dimensions=2\n"
    "        Level range: R[0, 1000]\n"
    "        Multisampling: Samples=0, Fixed sample locations=true\n"
    "        Swizzles: R=Red, G=Green, B=Blue, A=Alpha\n"
    "        Sampler: ION Sampler \"Sampler\" {\n"
    "          Autogenerating mipmaps: false\n"
    "          Texture compare mode: None\n"
    "          Texture compare function: Less\n"
    "          MinFilter mode: Nearest\n"
    "          MagFilter mode: Nearest\n"
    "          Level-of-detail range: R[-1000, 1000]\n"
    "          Wrap modes: R=Repeat, S=Repeat, T=Repeat\n"
    "        }\n"
    "      }\n"
    "    }\n"
    "    ION Uniform {\n"
    "      Name: \"uFV2\"\n"
    "      Type: FloatVector2\n"
    "      Value 0: V[1, 2]\n"
    "      Value 1: V[3, 4]\n"
    "    }\n"
    "  }\n"
    "  ION Node \"Shapes\" {\n"
    "    Enabled: false\n"
    "    ION StateTable {\n"
    "      DepthTest: true\n"
    "      StencilTest: false\n"
    "      Clear Color: V[0.4, 0.5, 0.6, 0.7]\n"
    "      Clear Stencil Value: 152\n"
    "    }\n"
    "    ION Shape \"Lines Shape\" {\n"
    "      Primitive Type: Lines\n"
    "      # Vertex Ranges: 2\n"
    "      Range 0: Enabled=true, Range=R[0, 3]\n"
    "      Range 1: Enabled=true, Range=R[10, 23]\n"
    "      ION AttributeArray \"Vertex array\" {\n"
    "        Buffer Values: {\n"
    "          v 0: 1, [1, 2], [1, 0 | 0, 1], [1, 0, 0 | 0, 1, 0 | 0, 0, 1], "
    "[1, 0, 0, 0 | 0, 1, 0, 0 | 0, 0, 1, 0 | 0, 0, 0, 1]\n"
    "          v 1: 2, [2, 3], [2, 0 | 0, 2], [2, 0, 0 | 0, 2, 0 | 0, 0, 2], "
    "[2, 0, 0, 0 | 0, 2, 0, 0 | 0, 0, 2, 0 | 0, 0, 0, 2]\n"
    "          v 2: 3, [3, 4], [3, 0 | 0, 3], [3, 0, 0 | 0, 3, 0 | 0, 0, 3], "
    "[3, 0, 0, 0 | 0, 3, 0, 0 | 0, 0, 3, 0 | 0, 0, 0, 3]\n"
    "        }\n"
    "        ION Attribute (Nonbuffer) {\n"
    "          Name: \"aFloat\"\n"
    "          Enabled: true\n"
    "          Value: 1\n"
    "        }\n"
    "        ION Attribute (Nonbuffer) {\n"
    "          Name: \"aFV2\"\n"
    "          Enabled: true\n"
    "          Value: V[1, 2]\n"
    "        }\n"
    "        ION Attribute (Nonbuffer) {\n"
    "          Name: \"aFV3\"\n"
    "          Enabled: true\n"
    "          Value: V[1, 2, 3]\n"
    "        }\n"
    "        ION Attribute (Nonbuffer) {\n"
    "          Name: \"aFV4\"\n"
    "          Enabled: true\n"
    "          Value: V[1, 2, 3, 4]\n"
    "        }\n"
    "        ION Attribute (Nonbuffer) {\n"
    "          Name: \"aFM2\"\n"
    "          Enabled: true\n"
    "          Value: [[1, 2]\n"
    "                  [3, 4]]\n"
    "        }\n"
    "        ION Attribute (Nonbuffer) {\n"
    "          Name: \"aFM3\"\n"
    "          Enabled: true\n"
    "          Value: [[1, 2, 3]\n"
    "                  [4, 5, 6]\n"
    "                  [7, 8, 9]]\n"
    "        }\n"
    "        ION Attribute (Nonbuffer) {\n"
    "          Name: \"aFM4\"\n"
    "          Enabled: true\n"
    "          Value: [[1, 2, 3, 4]\n"
    "                  [5, 6, 7, 8]\n"
    "                  [9, 1, 2, 3]\n"
    "                  [4, 5, 6, 7]]\n"
    "        }\n"
    "        ION Attribute (Buffer) {\n"
    "          Name: \"aBOE1\"\n"
    "          Enabled: true\n"
    "          Normalized: false\n"
    "          Buffer: \"vertices\"\n"
    "        }\n"
    "        ION Attribute (Buffer) {\n"
    "          Name: \"aBOE2\"\n"
    "          Enabled: true\n"
    "          Normalized: false\n"
    "          Buffer: \"vertices\"\n"
    "        }\n"
    "        ION Attribute (Buffer) {\n"
    "          Name: \"aBOEm2\"\n"
    "          Enabled: true\n"
    "          Normalized: false\n"
    "          Buffer: \"vertices\"\n"
    "        }\n"
    "        ION Attribute (Buffer) {\n"
    "          Name: \"aBOEm3\"\n"
    "          Enabled: true\n"
    "          Normalized: false\n"
    "          Buffer: \"vertices\"\n"
    "        }\n"
    "        ION Attribute (Buffer) {\n"
    "          Name: \"aBOEm4\"\n"
    "          Enabled: true\n"
    "          Normalized: false\n"
    "          Buffer: \"vertices\"\n"
    "        }\n"
    "      }\n"
    "      ION IndexBuffer \"Indices #0\" {\n"
    "        Type: Byte\n"
    "        Target: Elementbuffer\n"
    "        Indices: [0 - 9: 0, 1, 2, 0, 1, 2, 0, 1, 2, 0,\n"
    "                  10 - 19: 1, 2, 0, 1, 2, 0, 1, 2, 0, 1,\n"
    "                  20 - 23: 2, 0, 1, 2]\n"
    "      }\n"
    "    }\n"
    "    ION Shape \"Line loops Shape\" {\n"
    "      Primitive Type: Line Loop\n"
    "      # Vertex Ranges: 2\n"
    "      Range 0: Enabled=true, Range=R[0, 3]\n"
    "      Range 1: Enabled=true, Range=R[10, 23]\n"
    "      ION IndexBuffer \"Indices #1\" {\n"
    "        Type: Unsigned Byte\n"
    "        Target: Elementbuffer\n"
    "        Indices: [0 - 9: 0, 1, 2, 0, 1, 2, 0, 1, 2, 0,\n"
    "                  10 - 19: 1, 2, 0, 1, 2, 0, 1, 2, 0, 1,\n"
    "                  20 - 23: 2, 0, 1, 2]\n"
    "      }\n"
    "    }\n"
    "    ION Shape \"Line strips Shape\" {\n"
    "      Primitive Type: Line Strip\n"
    "      # Vertex Ranges: 2\n"
    "      Range 0: Enabled=true, Range=R[0, 3]\n"
    "      Range 1: Enabled=true, Range=R[10, 23]\n"
    "      ION IndexBuffer \"Indices #2\" {\n"
    "        Type: Short\n"
    "        Target: Elementbuffer\n"
    "        Indices: [0 - 9: 0, 1, 2, 0, 1, 2, 0, 1, 2, 0,\n"
    "                  10 - 19: 1, 2, 0, 1, 2, 0, 1, 2, 0, 1,\n"
    "                  20 - 23: 2, 0, 1, 2]\n"
    "      }\n"
    "    }\n"
    "    ION Shape \"Points Shape\" {\n"
    "      Primitive Type: Points\n"
    "      # Vertex Ranges: 2\n"
    "      Range 0: Enabled=true, Range=R[0, 3]\n"
    "      Range 1: Enabled=true, Range=R[10, 23]\n"
    "      ION IndexBuffer \"Indices #3\" {\n"
    "        Type: Unsigned Short\n"
    "        Target: Elementbuffer\n"
    "        Indices: [0 - 9: 0, 1, 2, 0, 1, 2, 0, 1, 2, 0,\n"
    "                  10 - 19: 1, 2, 0, 1, 2, 0, 1, 2, 0, 1,\n"
    "                  20 - 23: 2, 0, 1, 2]\n"
    "      }\n"
    "    }\n"
    "    ION Shape \"Triangles Shape\" {\n"
    "      Primitive Type: Triangles\n"
    "      # Vertex Ranges: 2\n"
    "      Range 0: Enabled=true, Range=R[0, 3]\n"
    "      Range 1: Enabled=true, Range=R[10, 23]\n"
    "      ION IndexBuffer \"Indices #4\" {\n"
    "        Type: Int\n"
    "        Target: Elementbuffer\n"
    "        Indices: [0 - 9: 0, 1, 2, 0, 1, 2, 0, 1, 2, 0,\n"
    "                  10 - 19: 1, 2, 0, 1, 2, 0, 1, 2, 0, 1,\n"
    "                  20 - 23: 2, 0, 1, 2]\n"
    "      }\n"
    "    }\n"
    "    ION Shape \"Triangle fans Shape\" {\n"
    "      Primitive Type: Triangle Fan\n"
    "      # Vertex Ranges: 2\n"
    "      Range 0: Enabled=true, Range=R[0, 3]\n"
    "      Range 1: Enabled=true, Range=R[10, 23]\n"
    "      ION IndexBuffer \"Indices #5\" {\n"
    "        Type: Unsigned Int\n"
    "        Target: Elementbuffer\n"
    "        Indices: [0 - 9: 0, 1, 2, 0, 1, 2, 0, 1, 2, 0,\n"
    "                  10 - 19: 1, 2, 0, 1, 2, 0, 1, 2, 0, 1,\n"
    "                  20 - 23: 2, 0, 1, 2]\n"
    "      }\n"
    "    }\n"
    "    ION Shape \"Triangle strips Shape\" {\n"
    "      Primitive Type: Triangle Strip\n"
    "      # Vertex Ranges: 2\n"
    "      Range 0: Enabled=true, Range=R[0, 3]\n"
    "      Range 1: Enabled=true, Range=R[10, 23]\n"
    "      ION IndexBuffer \"Indices #6\" {\n"
    "        Type: Float\n"
    "        Target: Elementbuffer\n"
    "        Indices: [0 - 9: 0, 1, 2, 0, 1, 2, 0, 1, 2, 0,\n"
    "                  10 - 19: 1, 2, 0, 1, 2, 0, 1, 2, 0, 1,\n"
    "                  20 - 23: 2, 0, 1, 2]\n"
    "      }\n"
    "    }\n"
    "    ION Shape {\n"
    "      Primitive Type: Lines\n"
    "      ION IndexBuffer {\n"
    "        Type: Invalid\n"
    "        Target: Elementbuffer\n"
    "        Indices: []\n"
    "      }\n"
    "    }\n"
    "    ION Shape {\n"
    "      Primitive Type: Points\n"
    "      ION IndexBuffer {\n"
    "        Type: Byte\n"
    "        Target: Elementbuffer\n"
    "        Indices: [0 - 0: [NULL]]\n"
    "      }\n"
    "    }\n"
    "    ION Shape {\n"
    "      Primitive Type: Points\n"
    "    }\n"
    "    ION Shape {\n"
    "      Primitive Type: Points\n"
    "      # Vertex Ranges: 1\n"
    "      Range 0: Enabled=true, Range=R[0, 3]\n"
    "    }\n"
    "  }\n"
    "}\n";

// A string that is the output HTML when a TestScene is printed.
static const char kTestSceneHtml[] =
    "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-0\" "
    "class=\"tree_expandbox\"/><label "
    "for=\"list-0\">ION Node \"Root\"</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">"
    "<input type=\"checkbox\" id=\"Root\" class=\"button\" checked></td></tr>\n"
    "<tr><td class=\"name\">Shader ID</td><td class=\"value\">\"Dummy "
    "Shader\"</td></tr>\n"
    "</table>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-1\" "
    "class=\"tree_expandbox\"/><label "
    "for=\"list-1\">ION StateTable</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">DepthTest</td><td "
    "class=\"value\">true</td></tr>\n"
    "<tr><td class=\"name\">StencilTest</td><td "
    "class=\"value\">false</td></tr>\n"
    "<tr><td class=\"name\">Clear Color</td><td class=\"value\">V[0.4, 0.5, "
    "0.6, 0.7]</td></tr>\n"
    "<tr><td class=\"name\">Clear Stencil Value</td><td "
    "class=\"value\">152</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-2\" "
    "class=\"tree_expandbox\"/><label "
    "for=\"list-2\">ION Uniform</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td class=\"value\">\"uFV2\"</td></tr>\n"
    "<tr><td class=\"name\">Type</td><td "
    "class=\"value\">FloatVector2</td></tr>\n"
    "<tr><td class=\"name\">Value</td><td class=\"value\">V[2, 3]</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-3\" "
    "class=\"tree_expandbox\"/><label "
    "for=\"list-3\">ION Uniform</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td "
    "class=\"value\">\"uMat2\"</td></tr>\n"
    "<tr><td class=\"name\">Type</td><td "
    "class=\"value\">Matrix2x2</td></tr>\n"
    "<tr><td class=\"name\">Value</td><td class=\"value\"><table "
    "class=\"nodes_field_value_table\">\n"
    "<tr>\n"
    "<td>1</td>\n"
    "<td>2</td>\n"
    "</tr>\n"
    "<tr>\n"
    "<td>3</td>\n"
    "<td>4</td>\n"
    "</tr>\n"
    "</table>\n"
    "</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-4\" "
    "class=\"tree_expandbox\"/><label "
    "for=\"list-4\">ION UniformBlock \"Block 1\"</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">true</td></tr>\n"
    "</table>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-5\" "
    "class=\"tree_expandbox\"/><label "
    "for=\"list-5\">ION Uniform</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td class=\"value\">\"uInt\"</td></tr>\n"
    "<tr><td class=\"name\">Type</td><td class=\"value\">Int</td></tr>\n"
    "<tr><td class=\"name\">Value</td><td class=\"value\">13</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-6\" "
    "class=\"tree_expandbox\"/><label "
    "for=\"list-6\">ION UniformBlock</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">true</td></tr>\n"
    "</table>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-7\" "
    "class=\"tree_expandbox\"/><label "
    "for=\"list-7\">ION Uniform</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td "
    "class=\"value\">\"uCubeTex\"</td></tr>\n"
    "<tr><td class=\"name\">Type</td><td "
    "class=\"value\">CubeMapTexture</td></tr>\n"
    "<tr><td class=\"name\">Value</td><td class=\"value\"><li><input type "
    "=\"checkbox\" checked=\"checked\" id=\"list-8\" "
    "class=\"tree_expandbox\"/><label "
    "for=\"list-8\">ION CubeMapTexture \"Cubemap\"</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Image</td><td class=\"value\">Face=Negative X, "
    "Format=Rgb888, Width=2, Height=2, Depth=1, Type=Dense, "
    "Dimensions=2</td></tr>\n"
    "<tr><td class=\"name\">Image</td><td class=\"value\">Face=Negative Y, "
    "Format=Rgb888, Width=2, Height=2, Depth=1, Type=Dense, "
    "Dimensions=2</td></tr>\n"
    "<tr><td class=\"name\">Image</td><td class=\"value\">Face=Negative Z, "
    "Format=Rgb888, Width=2, Height=2, Depth=1, Type=Dense, "
    "Dimensions=2</td></tr>\n"
    "<tr><td class=\"name\">Image</td><td class=\"value\">Face=Positive X, "
    "Format=Rgb888, Width=2, Height=2, Depth=1, Type=Dense, "
    "Dimensions=2</td></tr>\n"
    "<tr><td class=\"name\">Image</td><td class=\"value\">Face=Positive Y, "
    "Format=Rgb888, Width=2, Height=2, Depth=1, Type=Dense, "
    "Dimensions=2</td></tr>\n"
    "<tr><td class=\"name\">Image</td><td class=\"value\">Face=Positive Z, "
    "Format=Rgb888, Width=2, Height=2, Depth=1, Type=Dense, "
    "Dimensions=2</td></tr>\n"
    "<tr><td class=\"name\">Level range</td><td class=\"value\">R[10, "
    "100]</td></tr>\n"
    "<tr><td class=\"name\">Multisampling</td>"
    "<td class=\"value\">Samples=0, Fixed sample locations=true</td></tr>\n"
    "<tr><td class=\"name\">Swizzles</td><td class=\"value\">R=Alpha, "
    "G=Blue, B=Green, A=Red</td></tr>\n"
    "<tr><td class=\"name\">Sampler</td><td class=\"value\"><li><input type "
    "=\"checkbox\" checked=\"checked\" id=\"list-9\" "
    "class=\"tree_expandbox\"/><label "
    "for=\"list-9\">ION Sampler \"Cubemap Sampler\"</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Autogenerating mipmaps</td><td "
    "class=\"value\">false</td></tr>\n"
    "<tr><td class=\"name\">Texture compare mode</td><td "
    "class=\"value\">CompareToTexture</td></tr>\n"
    "<tr><td class=\"name\">Texture compare function</td><td "
    "class=\"value\">Never</td></tr>\n"
    "<tr><td class=\"name\">MinFilter mode</td><td "
    "class=\"value\">LinearMipmapLinear</td></tr>\n"
    "<tr><td class=\"name\">MagFilter mode</td><td "
    "class=\"value\">Nearest</td></tr>\n"
    "<tr><td class=\"name\">Level-of-detail range</td><td "
    "class=\"value\">R[-1.5, 1.5]</td></tr>\n"
    "<tr><td class=\"name\">Wrap modes</td><td "
    "class=\"value\">R=ClampToEdge, S=MirroredRepeat, "
    "T=ClampToEdge</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-10\" class=\"tree_expandbox\"/>"
    "<label for=\"list-10\">ION Uniform</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td class=\"value\">\"uTex\"</td></tr>\n"
    "<tr><td class=\"name\">Type</td><td class=\"value\">Texture</td></tr>\n"
    "<tr><td class=\"name\">Value</td><td class=\"value\"><li><input type "
    "=\"checkbox\" checked=\"checked\" id=\"list-11\" "
    "class=\"tree_expandbox\"/><label "
    "for=\"list-11\">ION Texture \"Texture\"</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Image</td><td class=\"value\">Face=None, "
    "Format=Rgb888, Width=2, Height=2, Depth=1, Type=Dense, "
    "Dimensions=2</td></tr>\n"
    "<tr><td class=\"name\">Level range</td><td class=\"value\">R[0, "
    "1000]</td></tr>\n"
    "<tr><td class=\"name\">Multisampling</td>"
    "<td class=\"value\">Samples=0, Fixed sample locations=true</td></tr>\n"
    "<tr><td class=\"name\">Swizzles</td><td class=\"value\">R=Red, G=Green, "
    "B=Blue, A=Alpha</td></tr>\n"
    "<tr><td class=\"name\">Sampler</td><td class=\"value\"><li><input type "
    "=\"checkbox\" checked=\"checked\" id=\"list-12\" "
    "class=\"tree_expandbox\"/><label "
    "for=\"list-12\">ION Sampler \"Sampler\"</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Autogenerating mipmaps</td><td "
    "class=\"value\">false</td></tr>\n"
    "<tr><td class=\"name\">Texture compare mode</td><td "
    "class=\"value\">None</td></tr>\n"
    "<tr><td class=\"name\">Texture compare function</td><td "
    "class=\"value\">Less</td></tr>\n"
    "<tr><td class=\"name\">MinFilter mode</td><td "
    "class=\"value\">Nearest</td></tr>\n"
    "<tr><td class=\"name\">MagFilter mode</td><td "
    "class=\"value\">Nearest</td></tr>\n"
    "<tr><td class=\"name\">Level-of-detail range</td><td "
    "class=\"value\">R[-1000, 1000]</td></tr>\n"
    "<tr><td class=\"name\">Wrap modes</td><td class=\"value\">R=Repeat, "
    "S=Repeat, T=Repeat</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-13\" class=\"tree_expandbox\"/>"
    "<label for=\"list-13\">ION Node \"Shapes\"</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">"
    "<input type=\"checkbox\" id=\"Shapes\" class=\"button\" ></td></tr>\n"
    "</table>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-14\" class=\"tree_expandbox\"/>"
    "<label for=\"list-14\">ION Shape \"Lines "
    "Shape\"</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Primitive Type</td><td "
    "class=\"value\">Lines</td></tr>\n"
    "<tr><td class=\"name\"># Vertex Ranges</td><td "
    "class=\"value\">2</td></tr>\n"
    "<tr><td class=\"name\">Range 0</td><td class=\"value\">Enabled=true, "
    "Range=R[0, 3]</td></tr>\n"
    "<tr><td class=\"name\">Range 1</td><td class=\"value\">Enabled=true, "
    "Range=R[10, 23]</td></tr>\n"
    "</table>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-15\" class=\"tree_expandbox\"/>"
    "<label for=\"list-15\">ION AttributeArray \"Vertex "
    "array\"</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Buffer Values</td><td class=\"value\"><li><input "
    "type =\"checkbox\" checked=\"checked\" id=\"list-16\" "
    "class=\"tree_expandbox\"/><label "
    "for=\"list-16\"></label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">v 0</td><td class=\"value\">1, [1, 2], [1, 0 | "
    "0, 1], [1, 0, 0 | 0, 1, 0 | 0, 0, 1], [1, 0, 0, 0 | 0, 1, 0, 0 | 0, 0, "
    "1, 0 | 0, 0, 0, 1]</td></tr>\n"
    "<tr><td class=\"name\">v 1</td><td class=\"value\">2, [2, 3], [2, 0 | "
    "0, 2], [2, 0, 0 | 0, 2, 0 | 0, 0, 2], [2, 0, 0, 0 | 0, 2, 0, 0 | 0, 0, "
    "2, 0 | 0, 0, 0, 2]</td></tr>\n"
    "<tr><td class=\"name\">v 2</td><td class=\"value\">3, [3, 4], [3, 0 | "
    "0, 3], [3, 0, 0 | 0, 3, 0 | 0, 0, 3], [3, 0, 0, 0 | 0, 3, 0, 0 | 0, 0, "
    "3, 0 | 0, 0, 0, 3]</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "</td></tr>\n"
    "</table>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-17\" class=\"tree_expandbox\"/>"
    "<label for=\"list-17\">ION Attribute "
    "(Nonbuffer)</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td "
    "class=\"value\">\"aFloat\"</td></tr>\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">true</td></tr>\n"
    "<tr><td class=\"name\">Value</td><td class=\"value\">1</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-18\" class=\"tree_expandbox\"/>"
    "<label for=\"list-18\">ION Attribute "
    "(Nonbuffer)</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td class=\"value\">\"aFV2\"</td></tr>\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">true</td></tr>\n"
    "<tr><td class=\"name\">Value</td><td class=\"value\">V[1, 2]</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-19\" class=\"tree_expandbox\"/>"
    "<label for=\"list-19\">ION Attribute "
    "(Nonbuffer)</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td class=\"value\">\"aFV3\"</td></tr>\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">true</td></tr>\n"
    "<tr><td class=\"name\">Value</td><td class=\"value\">V[1, 2, "
    "3]</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-20\" class=\"tree_expandbox\"/>"
    "<label for=\"list-20\">ION Attribute "
    "(Nonbuffer)</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td class=\"value\">\"aFV4\"</td></tr>\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">true</td></tr>\n"
    "<tr><td class=\"name\">Value</td><td class=\"value\">V[1, 2, 3, "
    "4]</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-21\" class=\"tree_expandbox\"/>"
    "<label for=\"list-21\">ION Attribute "
    "(Nonbuffer)</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td class=\"value\">\"aFM2\"</td></tr>\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">true</td></tr>\n"
    "<tr><td class=\"name\">Value</td><td class=\"value\"><table "
    "class=\"nodes_field_value_table\">\n"
    "<tr>\n"
    "<td>1</td>\n"
    "<td>2</td>\n"
    "</tr>\n"
    "<tr>\n"
    "<td>3</td>\n"
    "<td>4</td>\n"
    "</tr>\n"
    "</table>\n"
    "</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-22\" class=\"tree_expandbox\"/>"
    "<label for=\"list-22\">ION Attribute "
    "(Nonbuffer)</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td class=\"value\">\"aFM3\"</td></tr>\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">true</td></tr>\n"
    "<tr><td class=\"name\">Value</td><td class=\"value\"><table "
    "class=\"nodes_field_value_table\">\n"
    "<tr>\n"
    "<td>1</td>\n"
    "<td>2</td>\n"
    "<td>3</td>\n"
    "</tr>\n"
    "<tr>\n"
    "<td>4</td>\n"
    "<td>5</td>\n"
    "<td>6</td>\n"
    "</tr>\n"
    "<tr>\n"
    "<td>7</td>\n"
    "<td>8</td>\n"
    "<td>9</td>\n"
    "</tr>\n"
    "</table>\n"
    "</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-23\" class=\"tree_expandbox\"/>"
    "<label for=\"list-23\">ION Attribute "
    "(Nonbuffer)</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td class=\"value\">\"aFM4\"</td></tr>\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">true</td></tr>\n"
    "<tr><td class=\"name\">Value</td><td class=\"value\"><table "
    "class=\"nodes_field_value_table\">\n"
    "<tr>\n"
    "<td>1</td>\n"
    "<td>2</td>\n"
    "<td>3</td>\n"
    "<td>4</td>\n"
    "</tr>\n"
    "<tr>\n"
    "<td>5</td>\n"
    "<td>6</td>\n"
    "<td>7</td>\n"
    "<td>8</td>\n"
    "</tr>\n"
    "<tr>\n"
    "<td>9</td>\n"
    "<td>1</td>\n"
    "<td>2</td>\n"
    "<td>3</td>\n"
    "</tr>\n"
    "<tr>\n"
    "<td>4</td>\n"
    "<td>5</td>\n"
    "<td>6</td>\n"
    "<td>7</td>\n"
    "</tr>\n"
    "</table>\n"
    "</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-24\" class=\"tree_expandbox\"/>"
    "<label for=\"list-24\">ION Attribute "
    "(Buffer)</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td "
    "class=\"value\">\"aBOE1\"</td></tr>\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">true</td></tr>\n"
    "<tr><td class=\"name\">Normalized</td><td class=\"value\">false"
    "</td></tr>\n"
    "<tr><td class=\"name\">Buffer</td><td "
    "class=\"value\">\"vertices\"</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-25\" class=\"tree_expandbox\"/>"
    "<label for=\"list-25\">ION Attribute "
    "(Buffer)</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td "
    "class=\"value\">\"aBOE2\"</td></tr>\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">true</td></tr>\n"
    "<tr><td class=\"name\">Normalized</td><td class=\"value\">false"
    "</td></tr>\n"
    "<tr><td class=\"name\">Buffer</td><td "
    "class=\"value\">\"vertices\"</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-26\" class=\"tree_expandbox\"/>"
    "<label for=\"list-26\">ION Attribute "
    "(Buffer)</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td "
    "class=\"value\">\"aBOEm2\"</td></tr>\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">true</td></tr>\n"
    "<tr><td class=\"name\">Normalized</td><td class=\"value\">false"
    "</td></tr>\n"
    "<tr><td class=\"name\">Buffer</td><td "
    "class=\"value\">\"vertices\"</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-27\" class=\"tree_expandbox\"/>"
    "<label for=\"list-27\">ION Attribute "
    "(Buffer)</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td "
    "class=\"value\">\"aBOEm3\"</td></tr>\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">true</td></tr>\n"
    "<tr><td class=\"name\">Normalized</td><td class=\"value\">false"
    "</td></tr>\n"
    "<tr><td class=\"name\">Buffer</td><td "
    "class=\"value\">\"vertices\"</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-28\" class=\"tree_expandbox\"/>"
    "<label for=\"list-28\">ION Attribute "
    "(Buffer)</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Name</td><td "
    "class=\"value\">\"aBOEm4\"</td></tr>\n"
    "<tr><td class=\"name\">Enabled</td><td class=\"value\">true</td></tr>\n"
    "<tr><td class=\"name\">Normalized</td><td class=\"value\">false"
    "</td></tr>\n"
    "<tr><td class=\"name\">Buffer</td><td "
    "class=\"value\">\"vertices\"</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "</ul></li>\n"
    "<li><input type =\"checkbox\" checked=\"checked\" "
    "id=\"list-29\" class=\"tree_expandbox\"/>"
    "<label for=\"list-29\">ION IndexBuffer \"Line "
    "Indices\"</label><ul>\n"
    "<table class=\"nodes_field_table\">\n"
    "<tr><td class=\"name\">Type</td><td class=\"value\">Byte</td></tr>\n"
    "<tr><td class=\"name\">Target</td><td class=\"value\">"
    "Elementbuffer</td></tr>\n"
    "<tr><td class=\"name\">Indices</td><td class=\"value\"><table "
    "class=\"nodes_field_value_table\">\n"
    "<tr>\n"
    "<td><span class=\"table_label\">0 - 9</span></td><td>0</td>\n"
    "<td>1</td>\n"
    "<td>2</td>\n"
    "<td>0</td>\n"
    "<td>1</td>\n"
    "<td>2</td>\n"
    "<td>0</td>\n"
    "<td>1</td>\n"
    "<td>2</td>\n"
    "<td>0</td>\n"
    "</tr>\n"
    "<tr>\n"
    "<td><span class=\"table_label\">10 - 19</span></td><td>1</td>\n"
    "<td>2</td>\n"
    "<td>0</td>\n"
    "<td>1</td>\n"
    "<td>2</td>\n"
    "<td>0</td>\n"
    "<td>1</td>\n"
    "<td>2</td>\n"
    "<td>0</td>\n"
    "<td>1</td>\n"
    "</tr>\n"
    "<tr>\n"
    "<td><span class=\"table_label\">20 - 23</span></td><td>2</td>\n"
    "<td>0</td>\n"
    "<td>1</td>\n"
    "<td>2</td>\n"
    "<td></td>\n"
    "<td></td>\n"
    "<td></td>\n"
    "<td></td>\n"
    "<td></td>\n"
    "<td></td>\n"
    "</tr>\n"
    "</table>\n"
    "</td></tr>\n"
    "</table>\n"
    "</ul></li>\n"
    "</ul></li>\n"
    "</ul></li>\n"
    "</ul></li>\n";

// Vertex struct that contains one field of each attribute type.
struct Vertex {
  Vertex() {}
  // Convenience constructor that sets all fields to deterministic values.
  explicit Vertex(int i) :
      f(static_cast<float>(i) + 1.0f),
      fv2(f, f + 1.0f),
      fm2(Matrix2f::Identity() * f),
      fm3(Matrix3f::Identity() * f),
      fm4(Matrix4f::Identity() * f) {}
  float f;
  Vector2f fv2;
  Matrix2f fm2;
  Matrix3f fm3;
  Matrix4f fm4;
};

// Creates an array uniform in the passed registry from a vector of values.
template <typename T>
static Uniform CreateArrayUniform(const ShaderInputRegistryPtr& reg,
                                  const std::string& name,
                                  const std::vector<T>& values) {
  return reg->CreateArrayUniform(name, &values[0], values.size(),
                                 base::AllocatorPtr());
}

// Creates and returns a ShaderInputRegistry with one of each type of uniform
// and attribute in it.
static const ShaderInputRegistryPtr CreateRegistry() {
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);

  // One of each uniform type.
  reg->Add(ShaderInputRegistry::UniformSpec("uInt", gfx::kIntUniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uFloat", gfx::kFloatUniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uCubeTex", gfx::kCubeMapTextureUniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uUint", gfx::kUnsignedIntUniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uTex", gfx::kTextureUniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uFV2", gfx::kFloatVector2Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uFV3", gfx::kFloatVector3Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uFV4", gfx::kFloatVector4Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uIV2", gfx::kIntVector2Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uIV3", gfx::kIntVector3Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uIV4", gfx::kIntVector4Uniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uUV2", gfx::kUnsignedIntVector2Uniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uUV3", gfx::kUnsignedIntVector3Uniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uUV4", gfx::kUnsignedIntVector4Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uMat2", gfx::kMatrix2x2Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uMat3", gfx::kMatrix3x3Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uMat4", gfx::kMatrix4x4Uniform, "."));

  // One of each non-buffer attribute type.
  reg->Add(
      ShaderInputRegistry::AttributeSpec("aFloat", gfx::kFloatAttribute, "."));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aFV2", gfx::kFloatVector2Attribute, "."));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aFV3", gfx::kFloatVector3Attribute, "."));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aFV4", gfx::kFloatVector4Attribute, "."));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aFM2", gfx::kFloatMatrix2x2Attribute, "."));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aFM3", gfx::kFloatMatrix3x3Attribute, "."));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aFM4", gfx::kFloatMatrix4x4Attribute, "."));

  // A couple of buffer object element attributes.
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aBOE1", gfx::kBufferObjectElementAttribute, "."));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aBOE2", gfx::kBufferObjectElementAttribute, "."));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aBOEm2", gfx::kBufferObjectElementAttribute, "."));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aBOEm3", gfx::kBufferObjectElementAttribute, "."));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aBOEm4", gfx::kBufferObjectElementAttribute, "."));

  return reg;
}

// Creates and returns a StateTable that sets everything.
static const StateTablePtr CreateFullStateTable() {
  StateTablePtr st(new StateTable(200, 300));
  st->Enable(StateTable::kBlend, true);
  st->Enable(StateTable::kCullFace, true);
  st->Enable(StateTable::kDebugOutputSynchronous, true);
  st->Enable(StateTable::kDepthTest, true);
  st->Enable(StateTable::kDither, false);
  st->Enable(StateTable::kMultisample, false);
  st->Enable(StateTable::kPolygonOffsetFill, true);
  st->Enable(StateTable::kSampleAlphaToCoverage, true);
  st->Enable(StateTable::kSampleCoverage, false);
  st->Enable(StateTable::kScissorTest, true);
  st->Enable(StateTable::kStencilTest, false);
  st->SetBlendColor(Vector4f(0.1f, 0.2f, 0.3f, 0.4f));
  st->SetBlendEquations(StateTable::kSubtract, StateTable::kReverseSubtract);
  st->SetBlendFunctions(
      StateTable::kSrcColor, StateTable::kOneMinusDstColor,
      StateTable::kOneMinusConstantAlpha, StateTable::kDstColor);
  st->SetClearColor(Vector4f(0.4f, 0.5f, 0.6f, 0.7f));
  st->SetColorWriteMasks(false, true, true, false);
  st->SetCullFaceMode(StateTable::kCullFrontAndBack);
  st->SetFrontFaceMode(StateTable::kClockwise);
  st->SetClearDepthValue(0.2f);
  st->SetDepthFunction(StateTable::kDepthNotEqual);
  st->SetDepthRange(Range1f(0.2f, 0.6f));
  st->SetDepthWriteMask(false);
  st->SetHint(StateTable::kGenerateMipmapHint, StateTable::kHintNicest);
  st->SetLineWidth(0.4f);
  st->SetPolygonOffset(0.5f, 2.0f);
  st->SetSampleCoverage(0.4f, true);
  st->SetScissorBox(Range2i(Point2i(10, 20), Point2i(210, 320)));
  st->SetStencilFunctions(
      StateTable::kStencilNever, 10, 0x40404040,
      StateTable::kStencilLess, 5, 0x12345678);
  st->SetStencilOperations(
      StateTable::kStencilDecrement, StateTable::kStencilDecrementAndWrap,
      StateTable::kStencilIncrement, StateTable::kStencilIncrementAndWrap,
      StateTable::kStencilInvert, StateTable::kStencilReplace);
  st->SetClearStencilValue(152);
  st->SetStencilWriteMasks(0x12345678, 0xbeefface);
  st->SetViewport(Range2i(Point2i(10, 20), Point2i(210, 320)));

  return st;
}

// Creates and returns a StateTable that sets a few items.
static const StateTablePtr CreatePartialStateTable() {
  StateTablePtr st(new StateTable(200, 300));
  st->Enable(StateTable::kDepthTest, true);
  st->Enable(StateTable::kStencilTest, false);
  st->SetClearColor(Vector4f(0.4f, 0.5f, 0.6f, 0.7f));
  st->SetClearStencilValue(152);
  return st;
}

// Creates and returns a dummy ShaderProgram using a registry.
static const ShaderProgramPtr CreateShaderProgram(
    const ShaderInputRegistryPtr& reg_ptr) {
  ShaderProgramPtr program(new ShaderProgram(reg_ptr));
  program->SetLabel("Dummy Shader");
  program->SetVertexShader(ShaderPtr(new Shader("Dummy Vertex Shader Source")));
  program->SetFragmentShader(
      ShaderPtr(new Shader("Dummy Fragment Shader Source")));
  return program;
}

// Creates and returns a cube map containing 6 Images.
static const CubeMapTexturePtr CreateCubeMapTexture() {
  static const uint8 kPixels[2 * 2 * 3] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b };
  ImagePtr image(new Image);
  image->Set(Image::kRgb888, 2, 2,
             base::DataContainer::CreateAndCopy<uint8>(kPixels, 12, false,
                                                       image->GetAllocator()));

  CubeMapTexturePtr tex(new CubeMapTexture);
  SamplerPtr sampler(new Sampler);
  sampler->SetLabel("Cubemap Sampler");
  sampler->SetCompareFunction(Sampler::kNever);
  sampler->SetCompareMode(Sampler::kCompareToTexture);
  sampler->SetMinLod(-1.5f);
  sampler->SetMaxLod(1.5f);
  sampler->SetMinFilter(Sampler::kLinearMipmapLinear);
  sampler->SetMagFilter(Sampler::kNearest);
  sampler->SetWrapR(Sampler::kClampToEdge);
  sampler->SetWrapS(Sampler::kMirroredRepeat);
  sampler->SetWrapT(Sampler::kClampToEdge);
  tex->SetBaseLevel(10);
  tex->SetMaxLevel(100);
  for (int i = 0; i < 6; ++i)
    tex->SetImage(static_cast<CubeMapTexture::CubeFace>(i), 0U, image);
  tex->SetSampler(sampler);
  tex->SetLabel("Cubemap");
  tex->SetSwizzleRed(Texture::kAlpha);
  tex->SetSwizzleGreen(Texture::kBlue);
  tex->SetSwizzleBlue(Texture::kGreen);
  tex->SetSwizzleAlpha(Texture::kRed);
  return tex;
}

// Creates and returns a Texture containing an Image.
static const TexturePtr CreateTexture() {
  static const uint8 kPixels[2 * 2 * 3] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b };
  ImagePtr image(new Image);
  image->Set(Image::kRgb888, 2, 2,
             base::DataContainer::CreateAndCopy<uint8>(kPixels, 4, false,
                                                       image->GetAllocator()));

  TexturePtr tex(new Texture);
  SamplerPtr sampler(new Sampler);
  sampler->SetLabel("Sampler");
  tex->SetSampler(sampler);
  tex->SetLabel("Texture");
  tex->SetImage(0U, image);
  return tex;
}

// Adds a uniform of each type (including an invalid one) to a node.
static void AddUniformsToNode(const ShaderInputRegistryPtr& reg,
                              const NodePtr& node) {
  std::vector<int> ints;
  ints.push_back(1);
  ints.push_back(2);
  std::vector<float> floats;
  floats.push_back(1.f);
  floats.push_back(2.f);
  std::vector<uint32> uints;
  uints.push_back(3U);
  uints.push_back(4U);
  std::vector<TexturePtr> textures;
  textures.push_back(TexturePtr(CreateTexture()));
  textures.push_back(TexturePtr(CreateTexture()));
  std::vector<CubeMapTexturePtr> cubemaps;
  cubemaps.push_back(CubeMapTexturePtr(CreateCubeMapTexture()));
  cubemaps.push_back(CubeMapTexturePtr(CreateCubeMapTexture()));
  std::vector<math::Vector2i> vector2is;
  vector2is.push_back(math::Vector2i(1, 2));
  vector2is.push_back(math::Vector2i(3, 4));
  std::vector<math::Vector3i> vector3is;
  vector3is.push_back(math::Vector3i(1, 2, 3));
  vector3is.push_back(math::Vector3i(4, 5, 6));
  std::vector<math::Vector4i> vector4is;
  vector4is.push_back(math::Vector4i(1, 2, 3, 4));
  vector4is.push_back(math::Vector4i(5, 6, 7, 8));
  std::vector<math::Vector2ui> vector2uis;
  vector2uis.push_back(math::Vector2ui(1, 2));
  vector2uis.push_back(math::Vector2ui(3, 4));
  std::vector<math::Vector3ui> vector3uis;
  vector3uis.push_back(math::Vector3ui(1, 2, 3));
  vector3uis.push_back(math::Vector3ui(4, 5, 6));
  std::vector<math::Vector4ui> vector4uis;
  vector4uis.push_back(math::Vector4ui(1, 2, 3, 4));
  vector4uis.push_back(math::Vector4ui(5, 6, 7, 8));
  std::vector<math::Vector2f> vector2fs;
  vector2fs.push_back(math::Vector2f(1.f, 2.f));
  vector2fs.push_back(math::Vector2f(3.f, 4.f));
  std::vector<math::Vector3f> vector3fs;
  vector3fs.push_back(math::Vector3f(1.f, 2.f, 3.f));
  vector3fs.push_back(math::Vector3f(4.f, 5.f, 6.f));
  std::vector<math::Vector4f> vector4fs;
  vector4fs.push_back(math::Vector4f(1.f, 2.f, 3.f, 4.f));
  vector4fs.push_back(math::Vector4f(5.f, 6.f, 7.f, 8.f));
  std::vector<math::Matrix2f> matrix2fs;
  matrix2fs.push_back(math::Matrix2f::Identity());
  matrix2fs.push_back(math::Matrix2f::Identity() * 2.f);
  std::vector<math::Matrix3f> matrix3fs;
  matrix3fs.push_back(math::Matrix3f::Identity());
  matrix3fs.push_back(math::Matrix3f::Identity() * 2.f);
  std::vector<math::Matrix4f> matrix4fs;
  matrix4fs.push_back(math::Matrix4f::Identity());
  matrix4fs.push_back(math::Matrix4f::Identity() * 2.f);

  UniformBlockPtr block1(new UniformBlock);
  block1->SetLabel("Block 1");
  UniformBlockPtr block2(new UniformBlock);
  node->AddUniformBlock(block1);
  node->AddUniformBlock(block2);
  block1->AddUniform(reg->Create<Uniform>("uInt", 13));
  block1->AddUniform(reg->Create<Uniform>("uFloat", 1.5f));
  block1->AddUniform(reg->Create<Uniform>("uUint", 15U));
  block2->AddUniform(reg->Create<Uniform>("uCubeTex", CreateCubeMapTexture()));
  block2->AddUniform(reg->Create<Uniform>("uTex", CreateTexture()));
  block2->AddUniform(reg->Create<Uniform>("uFV2", Vector2f(2.f, 3.f)));
  node->AddUniform(reg->Create<Uniform>("uFV3", Vector3f(4.f, 5.f, 6.f)));
  node->AddUniform(reg->Create<Uniform>("uFV4", Vector4f(7.f, 8.f, 9.f, 10.f)));
  node->AddUniform(reg->Create<Uniform>("uIV2", Vector2i(2, 3)));
  node->AddUniform(reg->Create<Uniform>("uIV3", Vector3i(4, 5, 6)));
  node->AddUniform(reg->Create<Uniform>("uIV4", Vector4i(7, 8, 9, 10)));
  node->AddUniform(reg->Create<Uniform>("uUV2", Vector2ui(2U, 3U)));
  node->AddUniform(reg->Create<Uniform>("uUV3", Vector3ui(4U, 5U, 6U)));
  node->AddUniform(reg->Create<Uniform>("uUV4", Vector4ui(7U, 8U, 9U, 10U)));
  node->AddUniform(reg->Create<Uniform>("uMat2", Matrix2f(1.f, 2.f,
                                                          3.f, 4.f)));
  node->AddUniform(reg->Create<Uniform>("uMat3", Matrix3f(1.f, 2.f, 3.f,
                                                          4.f, 5.f, 6.f,
                                                          7.f, 8.f, 9.f)));
  node->AddUniform(reg->Create<Uniform>("uMat4", Matrix4f(1.f, 2.f, 3.f, 4.f,
                                                          5.f, 6.f, 7.f, 8.f,
                                                          9.f, 1.f, 2.f, 3.f,
                                                          4.f, 5.f, 6.f, 7.f)));

  // Array uniforms.
  block1->AddUniform(CreateArrayUniform(reg, "uInt", ints));
  block1->AddUniform(CreateArrayUniform(reg, "uFloat", floats));
  block1->AddUniform(CreateArrayUniform(reg, "uUint", uints));
  block2->AddUniform(CreateArrayUniform(reg, "uCubeTex", cubemaps));
  block2->AddUniform(CreateArrayUniform(reg, "uTex", textures));
  block2->AddUniform(CreateArrayUniform(reg, "uFV2", vector2fs));
  node->AddUniform(CreateArrayUniform(reg, "uFV3", vector3fs));
  node->AddUniform(CreateArrayUniform(reg, "uFV4", vector4fs));
  node->AddUniform(CreateArrayUniform(reg, "uIV2", vector2is));
  node->AddUniform(CreateArrayUniform(reg, "uIV3", vector3is));
  node->AddUniform(CreateArrayUniform(reg, "uIV4", vector4is));
  node->AddUniform(CreateArrayUniform(reg, "uUV2", vector2uis));
  node->AddUniform(CreateArrayUniform(reg, "uUV3", vector3uis));
  node->AddUniform(CreateArrayUniform(reg, "uUV4", vector4uis));
  node->AddUniform(CreateArrayUniform(reg, "uMat2", matrix2fs));
  node->AddUniform(CreateArrayUniform(reg, "uMat3", matrix3fs));
  node->AddUniform(CreateArrayUniform(reg, "uMat4", matrix4fs));

  // Try to add an invalid uniform for better coverage.
  Uniform invalid;
  node->AddUniform(invalid);
}

// Creates and returns a Shape with the given primitive type.
static const ShapePtr CreateShape(Shape::PrimitiveType prim_type) {
  ShapePtr shape(new Shape);
  shape->SetPrimitiveType(prim_type);
  return shape;
}

// Adds one Shape with each primitive type to a node.
static void AddShapesToNode(const NodePtr& node) {
  node->AddShape(CreateShape(Shape::kLines));
  node->GetShapes()[node->GetShapes().size() - 1U]->SetLabel("Lines Shape");
  node->AddShape(CreateShape(Shape::kLineLoop));
  node->GetShapes()[node->GetShapes().size() - 1U]
      ->SetLabel("Line loops Shape");
  node->AddShape(CreateShape(Shape::kLineStrip));
  node->GetShapes()[node->GetShapes().size() - 1U]
      ->SetLabel("Line strips Shape");
  node->AddShape(CreateShape(Shape::kPoints));
  node->GetShapes()[node->GetShapes().size() - 1U]->SetLabel("Points Shape");
  node->AddShape(CreateShape(Shape::kTriangles));
  node->GetShapes()[node->GetShapes().size() - 1U]->SetLabel("Triangles Shape");
  node->AddShape(CreateShape(Shape::kTriangleFan));
  node->GetShapes()[node->GetShapes().size() - 1U]
      ->SetLabel("Triangle fans Shape");
  node->AddShape(CreateShape(Shape::kTriangleStrip));
  node->GetShapes()[node->GetShapes().size() - 1U]
      ->SetLabel("Triangle strips Shape");
}

// Creates and returns an AttributeArray with each type of attribute.
static const AttributeArrayPtr CreateAttributeArray(
    const ShaderInputRegistryPtr& reg) {
  AttributeArrayPtr aa(new AttributeArray);
  aa->SetLabel("Vertex array");
  aa->AddAttribute(reg->Create<Attribute>("aFloat", 1.0f));
  aa->AddAttribute(reg->Create<Attribute>("aFV2", Vector2f(1.f, 2.f)));
  aa->AddAttribute(reg->Create<Attribute>("aFV3", Vector3f(1.f, 2.f, 3.f)));
  aa->AddAttribute(reg->Create<Attribute>("aFV4",
                                         Vector4f(1.f, 2.f, 3.f, 4.f)));
  aa->AddAttribute(reg->Create<Attribute>("aFM2", Matrix2f(1.f, 2.f,
                                                           3.f, 4.f)));
  aa->AddAttribute(reg->Create<Attribute>("aFM3", Matrix3f(1.f, 2.f, 3.f,
                                                           4.f, 5.f, 6.f,
                                                           7.f, 8.f, 9.f)));
  aa->AddAttribute(reg->Create<Attribute>("aFM4",
                                          Matrix4f(1.f, 2.f, 3.f, 4.f,
                                                   5.f, 6.f, 7.f, 8.f,
                                                   9.f, 1.f, 2.f, 3.f,
                                                   4.f, 5.f, 6.f, 7.f)));

  // Add and bind a couple of buffer object elements.
  const Vertex vertices[3] = { Vertex(0), Vertex(1), Vertex(2) };
  BufferObjectPtr buffer_object(new BufferObject);
  base::DataContainerPtr container = base::DataContainer::CreateAndCopy<Vertex>(
      vertices, 3, false, buffer_object->GetAllocator());
  buffer_object->SetLabel("vertices");
  buffer_object->SetData(container, sizeof(vertices[0]), 3U,
                         BufferObject::kStaticDraw);
  Vertex v;
  BufferToAttributeBinder<Vertex>(v)
      .Bind(v.f, "aBOE1")
      .Bind(v.fv2, "aBOE2")
      .Bind(v.fm2, "aBOEm2")
      .Bind(v.fm3, "aBOEm3")
      .Bind(v.fm4, "aBOEm4")
      .Apply(reg, aa, buffer_object);
  return aa;
}

// Creates and returns an IndexBuffer with the given type of indices.
template <typename T>
static IndexBufferPtr CreateIndexBuffer(BufferObject::ComponentType type) {
  // Set up an array of indices of the correct type.
  T indices[kNumIndices];
  for (int i = 0; i < kNumIndices; ++i)
    indices[i] = static_cast<T>(i % 3);

  IndexBufferPtr index_buffer(new IndexBuffer);
  // Copy them into a DataContainer.
  base::DataContainerPtr container = base::DataContainer::CreateAndCopy<T>(
      indices, kNumIndices, false, index_buffer->GetAllocator());

  // Create an IndexBuffer using them with a couple of ranges.
  index_buffer->SetLabel("Index buffer");
  index_buffer->AddSpec(type, 1, 0);
  index_buffer->SetData(container, sizeof(indices[0]), kNumIndices,
                        BufferObject::kStaticDraw);
  return index_buffer;
}

// Creates and returns a test scene for printing.
static const NodePtr BuildTestScene() {
  // This ensures that no errors are produced while building the scene.
  base::LogChecker log_checker;

  // Create a registry with one of each type of uniform and attribute in it.
  ShaderInputRegistryPtr reg_ptr(CreateRegistry());

  // Create a root node and add a StateTable and ShaderProgram to it.
  NodePtr root(new Node);
  root->SetLabel("Root");
  root->SetStateTable(CreateFullStateTable());
  root->SetShaderProgram(CreateShaderProgram(reg_ptr));

  // Add one uniform of each supported type to the root.
  AddUniformsToNode(reg_ptr, root);

  // Add a child Node with a partial StateTable and shapes in it. Disable it to
  // test disabled-node printing.
  NodePtr node_with_shapes(new Node);
  node_with_shapes->SetLabel("Shapes");
  root->AddChild(node_with_shapes);
  node_with_shapes->SetStateTable(CreatePartialStateTable());
  AddShapesToNode(node_with_shapes);
  node_with_shapes->Enable(false);

  // Add an AttributeArray with one of each attribute type to the first Shape.
  const base::AllocVector<ShapePtr>& shapes = node_with_shapes->GetShapes();
  shapes[0]->SetAttributeArray(CreateAttributeArray(reg_ptr));

  // Add one IndexBuffer of each type to the Shapes.
  DCHECK_GE(shapes.size(), 7U);
  shapes[0]->SetIndexBuffer(CreateIndexBuffer<int8>(BufferObject::kByte));
  shapes[1]->SetIndexBuffer(
      CreateIndexBuffer<uint8>(BufferObject::kUnsignedByte));
  shapes[2]->SetIndexBuffer(
      CreateIndexBuffer<int16>(BufferObject::kShort));
  shapes[3]->SetIndexBuffer(
      CreateIndexBuffer<uint16>(BufferObject::kUnsignedShort));
  shapes[4]->SetIndexBuffer(CreateIndexBuffer<int32>(BufferObject::kInt));
  shapes[5]->SetIndexBuffer(
      CreateIndexBuffer<uint32>(BufferObject::kUnsignedInt));
  shapes[6]->SetIndexBuffer(CreateIndexBuffer<float>(BufferObject::kFloat));

  // Add a couple of vertex ranges to each Shape.
  for (int i = 0; i < 7; ++i) {
    shapes[i]->AddVertexRange(Range1i(0, 3));
    shapes[i]->AddVertexRange(Range1i(10, kNumIndices - 1));
    shapes[i]->GetIndexBuffer()->SetLabel("Indices #" +
                                          base::ValueToString(i));
  }

  // Create and add a Shape with an invalid IndexBuffer.
  {
    node_with_shapes->AddShape(CreateShape(Shape::kLines));
    IndexBufferPtr index_buffer(new IndexBuffer);
    index_buffer->AddSpec(BufferObject::kInvalid, 0, 0);
    node_with_shapes->GetShapes().back()->SetIndexBuffer(index_buffer);
  }

  // Create and add a Shape with a valid type but NULL IndexBuffer data.
  {
    node_with_shapes->AddShape(CreateShape(Shape::kPoints));
    IndexBufferPtr index_buffer(new IndexBuffer);
    index_buffer->AddSpec(BufferObject::kByte, 0, 0);
    index_buffer->SetData(base::DataContainer::Create<int8>(
                              nullptr, kNullFunction, false,
                              index_buffer->GetAllocator()),
                          1U, 1U, BufferObject::kStaticDraw);
    node_with_shapes->GetShapes().back()->SetIndexBuffer(index_buffer);
  }

  // Create and add two Shapes with no IndexBuffer, one with a vertex range and
  // one without.
  {
    node_with_shapes->AddShape(CreateShape(Shape::kPoints));
    node_with_shapes->AddShape(CreateShape(Shape::kPoints));
    node_with_shapes->GetShapes().back()->AddVertexRange(Range1i(0, 3));
  }

  EXPECT_FALSE(log_checker.HasAnyMessages());
  return root;
}

// Creates and returns a test scene for printing to HTML; this achieves 100%
// coverage without going overboard.
static const NodePtr BuildHtmlTestScene() {
  // This ensures that no errors are produced while building the scene.
  base::LogChecker log_checker;

  // Create a registry with one of each type of uniform and attribute in it.
  ShaderInputRegistryPtr reg(CreateRegistry());

  // Create a root node and add a StateTable and ShaderProgram to it.
  NodePtr root(new Node);
  root->SetLabel("Root");
  root->SetStateTable(CreatePartialStateTable());
  root->SetShaderProgram(CreateShaderProgram(reg));
  UniformBlockPtr block1(new UniformBlock);
  block1->SetLabel("Block 1");
  UniformBlockPtr block2(new UniformBlock);
  root->AddUniformBlock(block1);
  root->AddUniformBlock(block2);

  // Add some uniforms to the root.
  block1->AddUniform(reg->Create<Uniform>("uInt", 13));
  block2->AddUniform(reg->Create<Uniform>("uCubeTex", CreateCubeMapTexture()));
  block2->AddUniform(reg->Create<Uniform>("uTex", CreateTexture()));
  root->AddUniform(reg->Create<Uniform>("uFV2", Vector2f(2.f, 3.f)));
  root->AddUniform(reg->Create<Uniform>("uMat2", Matrix2f(1.f, 2.f,
                                                          3.f, 4.f)));

  // Add a child Node with a shape in it. Disable it to test disabled-node
  // printing.
  NodePtr node_with_shapes(new Node);
  node_with_shapes->SetLabel("Shapes");
  node_with_shapes->Enable(false);
  root->AddChild(node_with_shapes);
  ShapePtr shape = CreateShape(Shape::kLines);
  shape->SetLabel("Lines Shape");
  node_with_shapes->AddShape(shape);

  // Add an AttributeArray with one of each attribute type to the Shape.
  shape->SetAttributeArray(CreateAttributeArray(reg));

  // Add an IndexBuffer to the Shape.
  shape->SetIndexBuffer(CreateIndexBuffer<int8>(BufferObject::kByte));
  shape->GetIndexBuffer()->SetLabel("Line Indices");

  // Add a couple of vertex ranges to the Shape.
  shape->AddVertexRange(Range1i(0, 3));
  shape->AddVertexRange(Range1i(10, kNumIndices - 1));

  EXPECT_FALSE(log_checker.HasAnyMessages());
  return root;
}

// Returns the expected string from printing the test scene as text.
static const std::string GetTestSceneTextString() {
  return std::string(kTestSceneText);
}

// Returns the expected string from printing the test scene as HTML.
static const std::string GetTestSceneHtmlString() {
  return std::string(kTestSceneHtml);
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// The tests.
//
//-----------------------------------------------------------------------------

TEST(PrinterTest, Flags) {
  Printer printer;

  // Check default settings.
  EXPECT_EQ(Printer::kText, printer.GetFormat());
  EXPECT_FALSE(printer.IsFullShapePrintingEnabled());
  EXPECT_TRUE(printer.IsAddressPrintingEnabled());

  // Check that the settings can be modified.
  printer.SetFormat(Printer::kHtml);
  EXPECT_EQ(Printer::kHtml, printer.GetFormat());
  printer.SetFormat(Printer::kText);
  EXPECT_EQ(Printer::kText, printer.GetFormat());
  printer.EnableFullShapePrinting(true);
  EXPECT_TRUE(printer.IsFullShapePrintingEnabled());
  printer.EnableFullShapePrinting(false);
  EXPECT_FALSE(printer.IsFullShapePrintingEnabled());
  printer.EnableAddressPrinting(true);
  EXPECT_TRUE(printer.IsAddressPrintingEnabled());
  printer.EnableAddressPrinting(false);
  EXPECT_FALSE(printer.IsAddressPrintingEnabled());
}

TEST(PrinterTest, AddressPrinting) {
  // The full scene-printing test would be difficult to write with addresses
  // printed, so it disables them. This tests a couple of addresses just to
  // make sure that path is covered.
  NodePtr node(new Node);
  ShapePtr shape(new Shape);
  node->AddShape(shape);

  // Also add a Texture uniform to test conditional field address printing.
  ShaderInputRegistryPtr reg(CreateRegistry());
  TexturePtr tex(new Texture);
  ImagePtr image(new Image);
  tex->SetImage(0U, image);
  size_t u = node->AddUniform(reg->Create<Uniform>("uTex", tex));

  // Build the expected strings.
  std::ostringstream out;
  out << "ION Node [" << node.Get()
      << ("] {\n"
          "  Enabled: true\n"
          "  ION Uniform [")
      << &node->GetUniforms()[u]
      << ("] {\n"
          "    Name: \"uTex\"\n"
          "    Type: Texture\n"
          "    Value: ION Texture [")
      << tex.Get()
      << ("] {\n"
          "      Image: Address=")
      << image.Get()
      << (", Face=None, Format=Rgb888, Width=0, Height=0, Depth=0, "
          "Type=Dense, Dimensions=2\n"
          "      Level range: R[0, 1000]\n"
          "      Multisampling: Samples=0, Fixed sample locations=true\n"
          "      Swizzles: R=Red, G=Green, B=Blue, A=Alpha\n"
          "      Sampler: ION Sampler [NULL] {\n"
          "      }\n"
          "    }\n"
          "  }\n"
          "  ION Shape [")
      << shape.Get()
      << ("] {\n"
          "    Primitive Type: Triangles\n"
          "  }\n"
          "}\n");
  const std::string expected_text = out.str();

  out.str("");
  out << ("<li><input type =\"checkbox\" checked=\"checked\""
          " id=\"list-0\" class=\"tree_expandbox\"/>"
          "<label for=\"list-0\">ION Node [")
      << node.Get()
      << ("]</label><ul>\n"
          "<table class=\"nodes_field_table\">\n"
          "<tr><td class=\"name\">Enabled</td>"
          "<td class=\"value\"><input type=\"checkbox\" id=\"\" "
          "class=\"button\" checked></td></tr>\n"
          "</table>\n"
          "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-1\" "
          "class=\"tree_expandbox\"/>"
          "<label for=\"list-1\">ION Uniform [")
      << &node->GetUniforms()[u]
      << ("]</label><ul>\n"
          "<table class=\"nodes_field_table\">\n"
          "<tr><td class=\"name\">Name</td>"
          "<td class=\"value\">\"uTex\"</td></tr>\n"
          "<tr><td class=\"name\">Type</td>"
          "<td class=\"value\">Texture</td></tr>\n"
          "<tr><td class=\"name\">Value</td><td class=\"value\">"
          "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-2\" "
          "class=\"tree_expandbox\"/>"
          "<label for=\"list-2\">ION Texture [")
      << tex.Get()
      << ("]</label><ul>\n"
          "<table class=\"nodes_field_table\">\n"
          "<tr><td class=\"name\">Image</td><td class=\"value\">Address=")
      << image.Get()
      << (", Face=None, Format=Rgb888, Width=0, Height=0, Depth=0, "
          "Type=Dense, Dimensions=2</td></tr>\n"
          "<tr><td class=\"name\">Level range</td>"
          "<td class=\"value\">R[0, 1000]</td></tr>\n"
          "<tr><td class=\"name\">Multisampling</td>"
          "<td class=\"value\">Samples=0, Fixed sample locations=true"
          "</td></tr>\n"
          "<tr><td class=\"name\">Swizzles</td>"
          "<td class=\"value\">R=Red, G=Green, B=Blue, A=Alpha</td></tr>\n"
          "<tr><td class=\"name\">Sampler</td><td class=\"value\">"
          "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-3\""
          " class=\"tree_expandbox\"/>"
          "<label for=\"list-3\">ION Sampler [NULL]</label><ul>\n"
          "</ul></li>\n</td></tr>\n</table>\n"
          "</ul></li>\n</td></tr>\n</table>\n</ul></li>\n"
          "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-4\""
          " class=\"tree_expandbox\"/>"
          "<label for=\"list-4\">ION Shape [")
      << shape.Get()
      << ("]</label><ul>\n"
          "<table class=\"nodes_field_table\">\n"
          "<tr><td class=\"name\">Primitive Type</td>"
          "<td class=\"value\">Triangles</td></tr>\n"
          "</table>\n</ul></li>\n</ul></li>\n");
  const std::string expected_html = out.str();
  out.str("");

  Printer printer;
  printer.EnableFullShapePrinting(true);
  printer.EnableAddressPrinting(true);

  // Use specialized string-matching function for more precise error messages.
  printer.SetFormat(Printer::kText);
  printer.PrintScene(node, out);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(expected_text, out.str()));
  out.str("");
  printer.SetFormat(Printer::kHtml);
  printer.PrintScene(node, out);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(expected_html, out.str()));
}

TEST(PrinterTest, PrintSceneAsText) {
  NodePtr root = BuildTestScene();
  std::ostringstream out;
  Printer printer;
  printer.SetFormat(Printer::kText);
  // Don't print addresses, as they are tricky to compare.
  printer.EnableAddressPrinting(false);
  printer.EnableFullShapePrinting(true);
  printer.PrintScene(root, out);

  // Use specialized string-matching function for more precise error messages.
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
                  GetTestSceneTextString(), out.str()));
}

TEST(PrinterTest, PrintSceneAsHtml) {
  NodePtr root = BuildHtmlTestScene();
  std::ostringstream out;
  Printer printer;
  printer.SetFormat(Printer::kHtml);
  // Don't print addresses, as they are tricky to compare.
  printer.EnableAddressPrinting(false);
  printer.EnableFullShapePrinting(true);
  printer.PrintScene(root, out);

  // Use specialized string-matching function for more precise error messages.
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
                  GetTestSceneHtmlString(), out.str()));
}

TEST(PrinterTest, PrintMultipleNodesAsHtml) {
  // This tests that multiple nodes in HTML result in unique list IDs.
  gfx::NodePtr node1(new gfx::Node);
  gfx::NodePtr node2(new gfx::Node);
  std::ostringstream out;
  Printer printer;
  printer.SetFormat(Printer::kHtml);
  printer.EnableAddressPrinting(false);
  printer.PrintScene(node1, out);
  printer.PrintScene(node2, out);

  const std::string expected =
      "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-0\" "
      "class=\"tree_expandbox\"/>"
      "<label for=\"list-0\">ION Node</label><ul>\n"
      "<table class=\"nodes_field_table\">\n"
      "<tr><td class=\"name\">Enabled</td><td class=\"value\">"
      "<input type=\"checkbox\" id=\"\" class=\"button\" checked></td></tr>\n"
      "</table>\n"
      "</ul></li>\n"
      "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-1\" "
      "class=\"tree_expandbox\"/>"
      "<label for=\"list-1\">ION Node</label><ul>\n"
      "<table class=\"nodes_field_table\">\n"
      "<tr><td class=\"name\">Enabled</td><td class=\"value\">"
      "<input type=\"checkbox\" id=\"\" class=\"button\" checked></td></tr>\n"
      "</table>\n"
      "</ul></li>\n";

  EXPECT_TRUE(base::testing::MultiLineStringsEqual(expected, out.str()));
}

}  // namespace gfxutils
}  // namespace ion
