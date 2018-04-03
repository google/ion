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

#include "ion/math/vector.h"

#include <sstream>
#include <string>

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace math {

//-----------------------------------------------------------------------------
// VectorBase class tests. These are tests that aren't specific to a single
// derived type.
// -----------------------------------------------------------------------------

TEST(Vector, VectorBaseAreValuesEqual) {
  // Two Vectors.
  EXPECT_TRUE(VectorBase4d::AreValuesEqual(Vector4d(1.5, 2.0, 6.5, -2.2),
                                           Vector4d(1.5, 2.0, 6.5, -2.2)));
  EXPECT_FALSE(VectorBase4d::AreValuesEqual(Vector4d(1.5, 2.0, 6.5, -2.2),
                                            Vector4d(1.5, 2.0, 6.5, -2.1)));

  // Vector and Point.
  EXPECT_TRUE(VectorBase3f::AreValuesEqual(Vector3f(-3.0f, 6.1f, 4.2f),
                                           Point3f(-3.0f, 6.1f, 4.2f)));
  EXPECT_FALSE(VectorBase3f::AreValuesEqual(Vector3f(-3.0f, 6.1f, 4.2f),
                                            Point3f(3.0f, 6.1f, 4.2f)));
}

//-----------------------------------------------------------------------------
// Vector class tests.
//-----------------------------------------------------------------------------

TEST(Vector, VectorDefaultConstructorZeroInitializes) {
  // Try the default constructor for a variety of element types and expect the
  // appropriate zeros.
  Vector1d v1d;
  EXPECT_EQ(0.0, v1d[0]);

  Vector1f v1f;
  EXPECT_EQ(0.0f, v1f[0]);

  Vector1i v1i;
  EXPECT_EQ(0, v1i[0]);

  // For a pointer type, zero-initialized means nullptr.  (Just to test
  // something other than a scalar type.)
  Vector<1, void*> v1p;
  EXPECT_EQ(nullptr, v1p[0]);

  // Test a vector with several elements to ensure they're all zeroed.
  Vector4d v4d;
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(0.0, v4d[i]);
  }
}

TEST(Vector, VectorConstructor) {
  Vector1i v1i(3);
  EXPECT_EQ(3, v1i[0]);

  Vector1f v1f(3.1f);
  EXPECT_EQ(3.1f, v1f[0]);

  Vector1d v1d(3.14);
  EXPECT_EQ(3.14, v1d[0]);

  Vector2i v2i(4, -5);
  EXPECT_EQ(4, v2i[0]);
  EXPECT_EQ(-5, v2i[1]);

  Vector2f v2f(6.1f, 7.2f);
  EXPECT_EQ(6.1f, v2f[0]);
  EXPECT_EQ(7.2f, v2f[1]);

  Vector2d v2d(9.4, 10.5);
  EXPECT_EQ(9.4, v2d[0]);
  EXPECT_EQ(10.5, v2d[1]);

  Vector3i v3i(12, 13, -14);
  EXPECT_EQ(12, v3i[0]);
  EXPECT_EQ(13, v3i[1]);
  EXPECT_EQ(-14, v3i[2]);

  Vector3f v3f(15.1f, 16.2f, -17.3f);
  EXPECT_EQ(15.1f, v3f[0]);
  EXPECT_EQ(16.2f, v3f[1]);
  EXPECT_EQ(-17.3f, v3f[2]);

  Vector3d v3d(18.4, 19.5, -20.6);
  EXPECT_EQ(18.4, v3d[0]);
  EXPECT_EQ(19.5, v3d[1]);
  EXPECT_EQ(-20.6, v3d[2]);

  Vector4i v4i(21, 22, 23, -24);
  EXPECT_EQ(21, v4i[0]);
  EXPECT_EQ(22, v4i[1]);
  EXPECT_EQ(23, v4i[2]);
  EXPECT_EQ(-24, v4i[3]);

  Vector4f v4f(25.1f, 26.2f, 27.3f, -28.4f);
  EXPECT_EQ(25.1f, v4f[0]);
  EXPECT_EQ(26.2f, v4f[1]);
  EXPECT_EQ(27.3f, v4f[2]);
  EXPECT_EQ(-28.4f, v4f[3]);

  Vector4d v4d(29.5, 30.6, 31.7, -32.8);
  EXPECT_EQ(29.5, v4d[0]);
  EXPECT_EQ(30.6, v4d[1]);
  EXPECT_EQ(31.7, v4d[2]);
  EXPECT_EQ(-32.8, v4d[3]);
}

TEST(Vector, VectorCompositeConstructor) {
  Vector2i v2i(Vector1i(4), -5);
  EXPECT_EQ(4, v2i[0]);
  EXPECT_EQ(-5, v2i[1]);

  Vector2f v2f(Vector1f(6.1f), 7.2f);
  EXPECT_EQ(6.1f, v2f[0]);
  EXPECT_EQ(7.2f, v2f[1]);

  Vector2d v2d(Vector1d(9.4), 10.5);
  EXPECT_EQ(9.4, v2d[0]);
  EXPECT_EQ(10.5, v2d[1]);

  Vector3i v3i(Vector2i(12, 13), -14);
  EXPECT_EQ(12, v3i[0]);
  EXPECT_EQ(13, v3i[1]);
  EXPECT_EQ(-14, v3i[2]);

  Vector3f v3f(Vector2f(15.1f, 16.2f), -17.3f);
  EXPECT_EQ(15.1f, v3f[0]);
  EXPECT_EQ(16.2f, v3f[1]);
  EXPECT_EQ(-17.3f, v3f[2]);

  Vector3d v3d(Vector2d(18.4, 19.5), -20.6);
  EXPECT_EQ(18.4, v3d[0]);
  EXPECT_EQ(19.5, v3d[1]);
  EXPECT_EQ(-20.6, v3d[2]);

  Vector4i v4i(Vector3i(21, 22, 23), -24);
  EXPECT_EQ(21, v4i[0]);
  EXPECT_EQ(22, v4i[1]);
  EXPECT_EQ(23, v4i[2]);
  EXPECT_EQ(-24, v4i[3]);

  Vector4f v4f(Vector3f(25.1f, 26.2f, 27.3f), -28.4f);
  EXPECT_EQ(25.1f, v4f[0]);
  EXPECT_EQ(26.2f, v4f[1]);
  EXPECT_EQ(27.3f, v4f[2]);
  EXPECT_EQ(-28.4f, v4f[3]);

  Vector4d v4d(Vector3d(29.5, 30.6, 31.7), -32.8);
  EXPECT_EQ(29.5, v4d[0]);
  EXPECT_EQ(30.6, v4d[1]);
  EXPECT_EQ(31.7, v4d[2]);
  EXPECT_EQ(-32.8, v4d[3]);
}

TEST(Vector, VectorTypeConvertingConstructor) {
  // Integer to float.
  EXPECT_EQ(Vector1f(12.0f), Vector1f(Vector1i(12)));
  // Integer to double.
  EXPECT_EQ(Vector2d(12.0, -13.0), Vector2d(Vector2i(12, -13)));
  // Float to double.
  EXPECT_EQ(Vector3d(12.0, -13.0, 14.5),
            Vector3d(Vector3f(12.0f, -13.0f, 14.5f)));
  // Double to integer.
  EXPECT_EQ(Vector4i(12, -13, 14, 15),
            Vector4i(Vector4d(12.1, -13.1, 14.2, 15.0)));
}

TEST(Vector, VectorEquals) {
  EXPECT_EQ(Vector2i(14, -15), Vector2i(14, -15));
  EXPECT_EQ(Vector2f(14.1f, -15.2f), Vector2f(14.1f, -15.2f));
  EXPECT_EQ(Vector2d(14.1, -15.2), Vector2d(14.1, -15.2));
  EXPECT_NE(Vector2i(14, -15), Vector2i(14, 15));
  EXPECT_NE(Vector2f(14.1f, -15.2f), Vector2f(14.1f, -15.6f));
  EXPECT_NE(Vector2d(14.1, -15.2), Vector2d(14.0, -15.2));

  EXPECT_EQ(Vector1i(14), Vector1i(14));
  EXPECT_EQ(Vector1f(14.1f), Vector1f(14.1f));
  EXPECT_EQ(Vector1d(14.1), Vector1d(14.1));
  EXPECT_NE(Vector1i(14), Vector1i(-14));
  EXPECT_NE(Vector1f(14.1f), Vector1f(-14.1f));
  EXPECT_NE(Vector1d(14.1), Vector1d(14.0));

  EXPECT_EQ(Vector3i(14, -15, 16), Vector3i(14, -15, 16));
  EXPECT_EQ(Vector3f(14.1f, -15.2f, 16.3f), Vector3f(14.1f, -15.2f, 16.3f));
  EXPECT_EQ(Vector3d(14.1, -15.2, 16.3), Vector3d(14.1, -15.2, 16.3));
  EXPECT_NE(Vector3i(14, -15, 16), Vector3i(14, 15, 16));
  EXPECT_NE(Vector3f(14.1f, -15.2f, 16.3f), Vector3f(14.1f, -15.6f, 16.3f));
  EXPECT_NE(Vector3d(14.1, -15.2, 16.3), Vector3d(14.0, -15.2, 16.3));

  EXPECT_EQ(Vector4i(14, -15, 16, -17), Vector4i(14, -15, 16, -17));
  EXPECT_EQ(Vector4f(14.1f, -15.2f, 16.3f, -17.4f),
            Vector4f(14.1f, -15.2f, 16.3f, -17.4f));
  EXPECT_EQ(Vector4d(14.1, -15.2, 16.3, -17.4),
            Vector4d(14.1, -15.2, 16.3, -17.4));
  EXPECT_NE(Vector4i(14, -15, 16, -17), Vector4i(14, -15, 16, 17));
  EXPECT_NE(Vector4f(14.1f, -15.2f, 16.3f, -17.4f),
            Vector4f(14.1f, -15.2f, 16.3f, -17.3f));
  EXPECT_NE(Vector4d(14.1, -15.2, 16.3, -17.4),
            Vector4d(14.1, -15.2, 16.3, -17.41));
}

TEST(Vector, VectorZero) {
  EXPECT_EQ(Vector1i(0), Vector1i::Zero());
  EXPECT_EQ(Vector1f(0.0f), Vector1f::Zero());
  EXPECT_EQ(Vector1d(0.0), Vector1d::Zero());

  EXPECT_EQ(Vector2i(0, 0), Vector2i::Zero());
  EXPECT_EQ(Vector2f(0.0f, 0.0f), Vector2f::Zero());
  EXPECT_EQ(Vector2d(0.0, 0.0), Vector2d::Zero());

  EXPECT_EQ(Vector3i(0, 0, 0), Vector3i::Zero());
  EXPECT_EQ(Vector3f(0.0f, 0.0f, 0.0f), Vector3f::Zero());
  EXPECT_EQ(Vector3d(0.0, 0.0, 0.0), Vector3d::Zero());

  EXPECT_EQ(Vector4i(0, 0, 0, 0), Vector4i::Zero());
  EXPECT_EQ(Vector4f(0.0f, 0.0f, 0.0f, 0.0f), Vector4f::Zero());
  EXPECT_EQ(Vector4d(0.0, 0.0, 0.0, 0.0), Vector4d::Zero());
}

TEST(Vector, VectorFill) {
  EXPECT_EQ(Vector1i(5), Vector1i::Fill(5));
  EXPECT_EQ(Vector1f(5.5f), Vector1f::Fill(5.5f));
  EXPECT_EQ(Vector1d(5.5), Vector1d::Fill(5.5));

  EXPECT_EQ(Vector2i(5, 5), Vector2i::Fill(5));
  EXPECT_EQ(Vector2f(5.5f, 5.5f), Vector2f::Fill(5.5f));
  EXPECT_EQ(Vector2d(5.5, 5.5), Vector2d::Fill(5.5));

  EXPECT_EQ(Vector3i(5, 5, 5), Vector3i::Fill(5));
  EXPECT_EQ(Vector3f(5.5f, 5.5f, 5.5f), Vector3f::Fill(5.5f));
  EXPECT_EQ(Vector3d(5.5, 5.5, 5.5), Vector3d::Fill(5.5));

  EXPECT_EQ(Vector4i(5, 5, 5, 5), Vector4i::Fill(5));
  EXPECT_EQ(Vector4f(5.5f, 5.5f, 5.5f, 5.5f), Vector4f::Fill(5.5f));
  EXPECT_EQ(Vector4d(5.5, 5.5, 5.5, 5.5), Vector4d::Fill(5.5));
}

TEST(Vector, VectorAxes) {
  EXPECT_EQ(Vector1i(1), Vector1i::AxisX());
  EXPECT_EQ(Vector1f(1.f), Vector1f::AxisX());
  EXPECT_EQ(Vector1d(1.), Vector1d::AxisX());

  EXPECT_EQ(Vector2i(1, 0), Vector2i::AxisX());
  EXPECT_EQ(Vector2f(1.f, 0.f), Vector2f::AxisX());
  EXPECT_EQ(Vector2d(1., 0.), Vector2d::AxisX());
  EXPECT_EQ(Vector2i(0, 1), Vector2i::AxisY());
  EXPECT_EQ(Vector2f(0.f, 1.f), Vector2f::AxisY());
  EXPECT_EQ(Vector2d(0., 1.), Vector2d::AxisY());

  EXPECT_EQ(Vector3i(1, 0, 0), Vector3i::AxisX());
  EXPECT_EQ(Vector3f(1.f, 0.f, 0.f), Vector3f::AxisX());
  EXPECT_EQ(Vector3d(1., 0., 0.), Vector3d::AxisX());
  EXPECT_EQ(Vector3i(0, 1, 0), Vector3i::AxisY());
  EXPECT_EQ(Vector3f(0.f, 1.f, 0.f), Vector3f::AxisY());
  EXPECT_EQ(Vector3d(0., 1., 0.), Vector3d::AxisY());
  EXPECT_EQ(Vector3i(0, 0, 1), Vector3i::AxisZ());
  EXPECT_EQ(Vector3f(0.f, 0.f, 1.f), Vector3f::AxisZ());
  EXPECT_EQ(Vector3d(0., 0., 1.), Vector3d::AxisZ());

  EXPECT_EQ(Vector4i(1, 0, 0, 0), Vector4i::AxisX());
  EXPECT_EQ(Vector4f(1.f, 0.f, 0.f, 0.f), Vector4f::AxisX());
  EXPECT_EQ(Vector4d(1., 0., 0., 0.), Vector4d::AxisX());
  EXPECT_EQ(Vector4i(0, 1, 0, 0), Vector4i::AxisY());
  EXPECT_EQ(Vector4f(0.f, 1.f, 0.f, 0.f), Vector4f::AxisY());
  EXPECT_EQ(Vector4d(0., 1., 0., 0.), Vector4d::AxisY());
  EXPECT_EQ(Vector4i(0, 0, 1, 0), Vector4i::AxisZ());
  EXPECT_EQ(Vector4f(0.f, 0.f, 1.f, 0.f), Vector4f::AxisZ());
  EXPECT_EQ(Vector4d(0., 0., 1., 0.), Vector4d::AxisZ());
  EXPECT_EQ(Vector4i(0, 0, 0, 1), Vector4i::AxisW());
  EXPECT_EQ(Vector4f(0.f, 0.f, 0.f, 1.f), Vector4f::AxisW());
  EXPECT_EQ(Vector4d(0., 0., 0., 1.), Vector4d::AxisW());
}

TEST(Vector, VectorSet) {
  Vector1i v1i = Vector1i::Zero();
  v1i.Set(2);
  EXPECT_EQ(Vector1i(2), v1i);

  Vector1f v1f = Vector1f::Zero();
  v1f.Set(3.1f);
  EXPECT_EQ(Vector1f(3.1f), v1f);

  Vector1d v1d = Vector1d::Zero();
  v1d.Set(7.2);
  EXPECT_EQ(Vector1d(7.2), v1d);

  Vector2i v2i = Vector2i::Zero();
  v2i.Set(4, -5);
  EXPECT_EQ(Vector2i(4, -5), v2i);

  Vector2f v2f = Vector2f::Zero();
  v2f.Set(6.1f, 7.2f);
  EXPECT_EQ(Vector2f(6.1f, 7.2f), v2f);

  Vector2d v2d = Vector2d::Zero();
  v2d.Set(9.4, 10.5);
  EXPECT_EQ(Vector2d(9.4, 10.5), v2d);

  Vector3i v3i = Vector3i::Zero();
  v3i.Set(12, 13, -14);
  EXPECT_EQ(Vector3i(12, 13, -14), v3i);

  Vector3f v3f = Vector3f::Zero();
  v3f.Set(15.1f, 16.2f, -17.3f);
  EXPECT_EQ(Vector3f(15.1f, 16.2f, -17.3f), v3f);

  Vector3d v3d = Vector3d::Zero();
  v3d.Set(18.4, 19.5, -20.6);
  EXPECT_EQ(Vector3d(18.4, 19.5, -20.6), v3d);

  Vector4i v4i = Vector4i::Zero();
  v4i.Set(21, 22, 23, -24);
  EXPECT_EQ(Vector4i(21, 22, 23, -24), v4i);

  Vector4f v4f = Vector4f::Zero();
  v4f.Set(25.1f, 26.2f, 27.3f, -28.4f);
  EXPECT_EQ(Vector4f(25.1f, 26.2f, 27.3f, -28.4f), v4f);

  Vector4d v4d = Vector4d::Zero();
  v4d.Set(29.5, 30.6, 31.7, -32.8);
  EXPECT_EQ(Vector4d(29.5, 30.6, 31.7, -32.8), v4d);
}

TEST(Vector, VectorAssign) {
  Vector1i v1i = Vector1i::Zero();
  EXPECT_EQ(Vector1i(2), v1i = Vector1i(2));
  EXPECT_EQ(Vector1i(2), v1i);

  Vector1f v1f = Vector1f::Zero();
  EXPECT_EQ(Vector1f(3.1f), v1f = Vector1f(3.1f));
  EXPECT_EQ(Vector1f(3.1f), v1f);

  Vector1d v1d = Vector1d::Zero();
  EXPECT_EQ(Vector1d(7.2), v1d = Vector1d(7.2));
  EXPECT_EQ(Vector1d(7.2), v1d);

  Vector2i v2i = Vector2i::Zero();
  EXPECT_EQ(Vector2i(4, -5), v2i = Vector2i(4, -5));
  EXPECT_EQ(Vector2i(4, -5), v2i);

  Vector2f v2f = Vector2f::Zero();
  EXPECT_EQ(Vector2f(6.1f, 7.2f), v2f = Vector2f(6.1f, 7.2f));
  EXPECT_EQ(Vector2f(6.1f, 7.2f), v2f);

  Vector2d v2d = Vector2d::Zero();
  EXPECT_EQ(Vector2d(9.4, 10.5), v2d = Vector2d(9.4, 10.5));
  EXPECT_EQ(Vector2d(9.4, 10.5), v2d);

  Vector3i v3i = Vector3i::Zero();
  EXPECT_EQ(Vector3i(12, 13, -14), v3i = Vector3i(12, 13, -14));
  EXPECT_EQ(Vector3i(12, 13, -14), v3i);

  Vector3f v3f = Vector3f::Zero();
  EXPECT_EQ(Vector3f(15.1f, 16.2f, -17.3f),
            v3f = Vector3f(15.1f, 16.2f, -17.3f));
  EXPECT_EQ(Vector3f(15.1f, 16.2f, -17.3f), v3f);

  Vector3d v3d = Vector3d::Zero();
  EXPECT_EQ(Vector3d(18.4, 19.5, -20.6), v3d = Vector3d(18.4, 19.5, -20.6));
  EXPECT_EQ(Vector3d(18.4, 19.5, -20.6), v3d);

  Vector4i v4i = Vector4i::Zero();
  EXPECT_EQ(Vector4i(21, 22, 23, -24), v4i = Vector4i(21, 22, 23, -24));
  EXPECT_EQ(Vector4i(21, 22, 23, -24), v4i);

  Vector4f v4f = Vector4f::Zero();
  EXPECT_EQ(Vector4f(25.1f, 26.2f, 27.3f, -28.4f),
            v4f = Vector4f(25.1f, 26.2f, 27.3f, -28.4f));
  EXPECT_EQ(Vector4f(25.1f, 26.2f, 27.3f, -28.4f), v4f);

  Vector4d v4d = Vector4d::Zero();
  EXPECT_EQ(Vector4d(29.5, 30.6, 31.7, -32.8),
            v4d = Vector4d(29.5, 30.6, 31.7, -32.8));
  EXPECT_EQ(Vector4d(29.5, 30.6, 31.7, -32.8), v4d);
}

TEST(Vector, VectorMutate) {
  Vector1i v1i = Vector1i::Zero();
  v1i[0] = 2;
  EXPECT_EQ(Vector1i(2), v1i);

  Vector1f v1f = Vector1f::Zero();
  v1f[0] = 3.2f;
  EXPECT_EQ(Vector1f(3.2f), v1f);

  Vector1d v1d = Vector1d::Zero();
  v1d[0] = 7.4;
  EXPECT_EQ(Vector1d(7.4), v1d);

  Vector2i v2i = Vector2i::Zero();
  v2i[0] = 4;
  v2i[1] = -5;
  EXPECT_EQ(Vector2i(4, -5), v2i);

  Vector2f v2f = Vector2f::Zero();
  v2f[0] = 6.1f;
  v2f[1] = 7.2f;
  EXPECT_EQ(Vector2f(6.1f, 7.2f), v2f);

  Vector2d v2d = Vector2d::Zero();
  v2d[0] = 9.4;
  v2d[1] = 10.5;
  EXPECT_EQ(Vector2d(9.4, 10.5), v2d);

  Vector3i v3i = Vector3i::Zero();
  v3i[0] = 12;
  v3i[1] = 13;
  v3i[2] = -14;
  EXPECT_EQ(Vector3i(12, 13, -14), v3i);

  Vector3f v3f = Vector3f::Zero();
  v3f[0] = 15.1f;
  v3f[1] = 16.2f;
  v3f[2] = -17.3f;
  EXPECT_EQ(Vector3f(15.1f, 16.2f, -17.3f), v3f);

  Vector3d v3d = Vector3d::Zero();
  v3d[0] = 18.4;
  v3d[1] = 19.5;
  v3d[2] = -20.6;
  EXPECT_EQ(Vector3d(18.4, 19.5, -20.6), v3d);

  Vector4i v4i = Vector4i::Zero();
  v4i[0] = 21;
  v4i[1] = 22;
  v4i[2] = 23;
  v4i[3] = -24;
  EXPECT_EQ(Vector4i(21, 22, 23, -24), v4i);

  Vector4f v4f = Vector4f::Zero();
  v4f[0] = 25.1f;
  v4f[1] = 26.2f;
  v4f[2] = 27.3f;
  v4f[3] = -28.4f;
  EXPECT_EQ(Vector4f(25.1f, 26.2f, 27.3f, -28.4f), v4f);

  Vector4d v4d = Vector4d::Zero();
  v4d[0] = 29.5;
  v4d[1] = 30.6;
  v4d[2] = 31.7;
  v4d[3] = -32.8;
  EXPECT_EQ(Vector4d(29.5, 30.6, 31.7, -32.8), v4d);
}

TEST(Vector, VectorData) {
  Vector1i v1i(2);
  EXPECT_EQ(2, v1i.Data()[0]);

  Vector1f v1f(3.2f);
  EXPECT_EQ(3.2f, v1f.Data()[0]);

  Vector1d v1d(7.3);
  EXPECT_EQ(7.3, v1d.Data()[0]);

  Vector2i v2i(4, -5);
  EXPECT_EQ(4, v2i.Data()[0]);
  EXPECT_EQ(-5, v2i.Data()[1]);

  Vector2f v2f(6.1f, 7.2f);
  EXPECT_EQ(6.1f, v2f.Data()[0]);
  EXPECT_EQ(7.2f, v2f.Data()[1]);

  Vector2d v2d(9.4, 10.5);
  EXPECT_EQ(9.4, v2d.Data()[0]);
  EXPECT_EQ(10.5, v2d.Data()[1]);

  Vector3i v3i(12, 13, -14);
  EXPECT_EQ(12, v3i.Data()[0]);
  EXPECT_EQ(13, v3i.Data()[1]);
  EXPECT_EQ(-14, v3i.Data()[2]);

  Vector3f v3f(15.1f, 16.2f, -17.3f);
  EXPECT_EQ(15.1f, v3f.Data()[0]);
  EXPECT_EQ(16.2f, v3f.Data()[1]);
  EXPECT_EQ(-17.3f, v3f.Data()[2]);

  Vector3d v3d(18.4, 19.5, -20.6);
  EXPECT_EQ(18.4, v3d.Data()[0]);
  EXPECT_EQ(19.5, v3d.Data()[1]);
  EXPECT_EQ(-20.6, v3d.Data()[2]);

  Vector4i v4i(21, 22, 23, -24);
  EXPECT_EQ(21, v4i.Data()[0]);
  EXPECT_EQ(22, v4i.Data()[1]);
  EXPECT_EQ(23, v4i.Data()[2]);
  EXPECT_EQ(-24, v4i.Data()[3]);

  Vector4f v4f(25.1f, 26.2f, 27.3f, -28.4f);
  EXPECT_EQ(25.1f, v4f.Data()[0]);
  EXPECT_EQ(26.2f, v4f.Data()[1]);
  EXPECT_EQ(27.3f, v4f.Data()[2]);
  EXPECT_EQ(-28.4f, v4f.Data()[3]);

  Vector4d v4d(29.5, 30.6, 31.7, -32.8);
  EXPECT_EQ(29.5, v4d.Data()[0]);
  EXPECT_EQ(30.6, v4d.Data()[1]);
  EXPECT_EQ(31.7, v4d.Data()[2]);
  EXPECT_EQ(-32.8, v4d.Data()[3]);
}

TEST(Vector, VectorSelfModifyingMathOperators) {
  Vector4d v(1.0, 2.0, 3.0, 4.0);

  v += Vector4d(7.5, 9.5, 11.5, 13.5);
  EXPECT_EQ(Vector4d(8.5, 11.5, 14.5, 17.5), v);

  v -= Vector4d(7.5, 9.5, 11.5, 13.5);
  EXPECT_EQ(Vector4d(1.0, 2.0, 3.0, 4.0), v);

  v *= 2.0;
  EXPECT_EQ(Vector4d(2.0, 4.0, 6.0, 8.0), v);

  v /= 4.0;
  EXPECT_EQ(Vector4d(0.5, 1.0, 1.5, 2.0), v);
}

TEST(Vector, VectorUnaryAndBinaryMathOperators) {
  Vector4d v0(1.5, 2.0, 6.5, -4.0);
  Vector4d v1(4.0, 5.5, 3.5, 7.0);
  Vector4d v2(3.0, 5.0, 3.25, 2.0);

  EXPECT_EQ(Vector4d(-1.5, -2.0, -6.5, 4.0), -v0);
  EXPECT_EQ(Vector4d(-4.0, -5.5, -3.5, -7.0), -v1);

  EXPECT_EQ(Vector4d(5.5, 7.5, 10.0, 3.0), v0 + v1);
  EXPECT_EQ(Vector4d(5.5, 7.5, 10.0, 3.0), v1 + v0);

  EXPECT_EQ(Vector4d(-2.5, -3.5, 3.0, -11.0), v0 - v1);
  EXPECT_EQ(Vector4d(-2.5, -3.5, 3.0, -11.0), v0 - v1);

  EXPECT_EQ(Vector4d(6.0, 8.0, 26.0, -16.0), v0 * 4.0);
  EXPECT_EQ(Vector4d(12.0, 16.5, 10.5, 21.0), 3.0 * v1);
  EXPECT_EQ(Vector4d(0.75, 1.0, 3.25, -2.0), v0 / 2.0);

  EXPECT_EQ(Vector2d(4.0, 3.0), 12.0 / Vector2d(3.0, 4.0));
  EXPECT_EQ(Vector3d(6.0, 4.0, 3.0), 12.0 / Vector3d(2.0, 3.0, 4.0));
  EXPECT_EQ(Vector4d(12.0, 6.0, 4.0, 3.0), 12.0 / Vector4d(1.0, 2.0, 3.0, 4.0));

  EXPECT_EQ(Vector4d(6.0, 11.0, 22.75, -28.0), v0 * v1);
  EXPECT_EQ(Vector4d(6.0, 11.0, 22.75, -28.0), v1 * v0);

  EXPECT_EQ(Vector4d(0.5, 0.4, 2.0, -2.0), v0 / v2);
  EXPECT_EQ(Vector4d(2.0, 2.5, 0.5, -0.5), v2 / v0);
}

TEST(Vector, VectorEqualityOperators) {
  EXPECT_TRUE(Vector4d(1.5, 2.0, 6.5, -2.2) == Vector4d(1.5, 2.0, 6.5, -2.2));
  EXPECT_FALSE(Vector4d(1.5, 2.0, 6.5, -2.2) == Vector4d(1.5, 2.0, 6.4, -2.2));
  EXPECT_FALSE(Vector4d(1.5, 2.0, 6.5, -2.2) == Vector4d(1.5, 2.1, 6.5, -2.2));
  EXPECT_FALSE(Vector4d(1.5, 2.0, 6.5, -2.2) == Vector4d(1.6, 2.0, 6.5, -2.2));
  EXPECT_FALSE(Vector4d(1.5, 2.0, 6.5, -2.2) == Vector4d(1.6, 2.0, 6.5, 2.2));

  EXPECT_FALSE(Vector4d(1.5, 2.0, 6.5, -2.2) != Vector4d(1.5, 2.0, 6.5, -2.2));
  EXPECT_TRUE(Vector4d(1.5, 2.0, 6.5, -2.2) != Vector4d(1.5, 2.0, 6.4, -2.2));
  EXPECT_TRUE(Vector4d(1.5, 2.0, 6.5, -2.2) != Vector4d(1.5, 2.1, 6.5, -2.2));
  EXPECT_TRUE(Vector4d(1.5, 2.0, 6.5, -2.2) != Vector4d(1.6, 2.0, 6.5, -2.2));
  EXPECT_TRUE(Vector4d(1.5, 2.0, 6.5, -2.2) != Vector4d(1.6, 2.0, 6.5, 2.2));
}

//-----------------------------------------------------------------------------
// Point class tests.
//
// Note: Because Point is very similar to Vector, this tests only one
// Dimension/Scalar template type rather than all of them, as Vector does.
// -----------------------------------------------------------------------------

TEST(Vector, PointConstructor) {
  Point4d p4d(29.5, 30.6, 31.7, -32.8);
  EXPECT_EQ(29.5, p4d[0]);
  EXPECT_EQ(30.6, p4d[1]);
  EXPECT_EQ(31.7, p4d[2]);
  EXPECT_EQ(-32.8, p4d[3]);
}

TEST(Vector, PointCompositeConstructor) {
  Point4d p4d(Point3d(29.5, 30.6, 31.7), -32.8);
  EXPECT_EQ(29.5, p4d[0]);
  EXPECT_EQ(30.6, p4d[1]);
  EXPECT_EQ(31.7, p4d[2]);
  EXPECT_EQ(-32.8, p4d[3]);
}

TEST(Vector, PointTypeConvertingConstructor) {
  // Integer to float.
  EXPECT_EQ(Point1f(12.0f), Point1f(Point1i(12)));
  // Integer to double.
  EXPECT_EQ(Point2d(12.0, -13.0), Point2d(Point2i(12, -13)));
  // Float to double.
  EXPECT_EQ(Point3d(12.0, -13.0, 14.5),
            Point3d(Point3f(12.0f, -13.0f, 14.5f)));
  // Double to integer.
  EXPECT_EQ(Point4i(12, -13, 14, 15),
            Point4i(Point4d(12.1, -13.1, 14.2, 15.0)));
}

TEST(Vector, PointEquals) {
  EXPECT_EQ(Point4d(14.1, -15.2, 16.3, -17.4),
            Point4d(14.1, -15.2, 16.3, -17.4));
  EXPECT_NE(Point4d(14.1, -15.2, 16.3, -17.4),
            Point4d(14.1, -15.2, 16.3, -17.41));
}

TEST(Vector, PointZero) {
  EXPECT_EQ(Point4d(0.0, 0.0, 0.0, 0.0), Point4d::Zero());
}

TEST(Vector, PointFill) {
  EXPECT_EQ(Point4d(1.2, 1.2, 1.2, 1.2), Point4d::Fill(1.2));
}

TEST(Vector, PointSet) {
  Point4d p4d = Point4d::Zero();
  p4d.Set(29.5, 30.6, 31.7, -32.8);
  EXPECT_EQ(Point4d(29.5, 30.6, 31.7, -32.8), p4d);
}

TEST(Vector, PointAssign) {
  Point4d p4d = Point4d::Zero();
  EXPECT_EQ(Point4d(29.5, 30.6, 31.7, -32.8),
            p4d = Point4d(29.5, 30.6, 31.7, -32.8));
  EXPECT_EQ(Point4d(29.5, 30.6, 31.7, -32.8), p4d);
}

TEST(Vector, PointMutate) {
  Point4d p4d = Point4d::Zero();
  p4d[0] = 29.5;
  p4d[1] = 30.6;
  p4d[2] = 31.7;
  p4d[3] = -32.8;
  EXPECT_EQ(Point4d(29.5, 30.6, 31.7, -32.8), p4d);
}

TEST(Vector, PointData) {
  Point4d p4d(29.5, 30.6, 31.7, -32.8);
  EXPECT_EQ(29.5, p4d.Data()[0]);
  EXPECT_EQ(30.6, p4d.Data()[1]);
  EXPECT_EQ(31.7, p4d.Data()[2]);
  EXPECT_EQ(-32.8, p4d.Data()[3]);
}

TEST(Vector, PointSelfModifyingMathOperators) {
  Point4d p(1.0, 2.0, 3.0, 4.0);

  p += Vector4d(7.5, 9.5, 11.5, 13.5);
  EXPECT_EQ(Point4d(8.5, 11.5, 14.5, 17.5), p);

  p -= Vector4d(7.5, 9.5, 11.5, 13.5);
  EXPECT_EQ(Point4d(1.0, 2.0, 3.0, 4.0), p);

  p *= 2.0;
  EXPECT_EQ(Point4d(2.0, 4.0, 6.0, 8.0), p);

  p /= 4.0;
  EXPECT_EQ(Point4d(0.5, 1.0, 1.5, 2.0), p);

  p += p;
  EXPECT_EQ(Point4d(1.0, 2.0, 3.0, 4.0), p);
}

TEST(Vector, PointUnaryAndBinaryMathOperators) {
  Point4d p0(1.5, 2.0, 6.5, -4.0);
  Point4d p1(4.0, 5.5, 3.5, 7.0);
  Vector4d v(3.0, -1.0, 2.0, -4.5);

  // Negation.
  EXPECT_EQ(Point4d(-1.5, -2.0, -6.5, 4.0), -p0);
  EXPECT_EQ(Point4d(-4.0, -5.5, -3.5, -7.0), -p1);

  // Point + Vector.
  EXPECT_EQ(Point4d(4.5, 1.0, 8.5, -8.5), p0 + v);
  EXPECT_EQ(Point4d(4.5, 1.0, 8.5, -8.5), v + p0);

  // Point + Point.
  EXPECT_EQ(Point4d(5.5, 7.5, 10.0, 3.0), p0 + p1);
  EXPECT_EQ(Point4d(5.5, 7.5, 10.0, 3.0), p1 + p0);

  // Point - Vector.
  EXPECT_EQ(Point4d(-1.5, 3.0, 4.5, 0.5), p0 - v);
  EXPECT_EQ(Point4d(1.0, 6.5, 1.5, 11.5), p1 - v);

  // Point - Point.
  EXPECT_EQ(Vector4d(-2.5, -3.5, 3.0, -11.0), p0 - p1);

  // Scaling.
  EXPECT_EQ(Point4d(6.0, 8.0, 26.0, -16.0), p0 * 4.0);
  EXPECT_EQ(Point4d(12.0, 16.5, 10.5, 21.0), 3.0 * p1);
  EXPECT_EQ(Point4d(0.75, 1.0, 3.25, -2.0), p0 / 2.0);
}

TEST(Vector, PointEqualityOperators) {
  EXPECT_TRUE(Point4d(1.5, 2.0, 6.5, -2.2) == Point4d(1.5, 2.0, 6.5, -2.2));
  EXPECT_FALSE(Point4d(1.5, 2.0, 6.5, -2.2) == Point4d(1.5, 2.0, 6.4, -2.2));
  EXPECT_FALSE(Point4d(1.5, 2.0, 6.5, -2.2) == Point4d(1.5, 2.1, 6.5, -2.2));
  EXPECT_FALSE(Point4d(1.5, 2.0, 6.5, -2.2) == Point4d(1.6, 2.0, 6.5, -2.2));
  EXPECT_FALSE(Point4d(1.5, 2.0, 6.5, -2.2) == Point4d(1.6, 2.0, 6.5, 2.2));

  EXPECT_FALSE(Point4d(1.5, 2.0, 6.5, -2.2) != Point4d(1.5, 2.0, 6.5, -2.2));
  EXPECT_TRUE(Point4d(1.5, 2.0, 6.5, -2.2) != Point4d(1.5, 2.0, 6.4, -2.2));
  EXPECT_TRUE(Point4d(1.5, 2.0, 6.5, -2.2) != Point4d(1.5, 2.1, 6.5, -2.2));
  EXPECT_TRUE(Point4d(1.5, 2.0, 6.5, -2.2) != Point4d(1.6, 2.0, 6.5, -2.2));
  EXPECT_TRUE(Point4d(1.5, 2.0, 6.5, -2.2) != Point4d(1.6, 2.0, 6.5, 2.2));
}

TEST(Vector, Streaming) {
  std::ostringstream out;
  out << Vector3d(1.5, 2.5, 3.5);
  EXPECT_EQ(std::string("V[1.5, 2.5, 3.5]"), out.str());
  out.str("");
  out << Point3d(4.5, 5.5, 6.5);
  EXPECT_EQ(std::string("P[4.5, 5.5, 6.5]"), out.str());

  {
    std::istringstream in("V[1.5, 2.5, 3.5]");
    Vector3d v(0., 0., 0.);
    in >> v;
    EXPECT_EQ(Vector3d(1.5, 2.5, 3.5), v);
  }

  {
    std::istringstream in("P[1.5, 2.5, 3.5]");
    Vector3d v(0., 0., 0.);
    in >> v;
    EXPECT_EQ(Vector3d(0., 0., 0.), v);
  }

  {
    std::istringstream in("P[1.5, 2.5, 3.5]");
    Point3d v(0., 0., 0.);
    in >> v;
    EXPECT_EQ(Point3d(1.5, 2.5, 3.5), v);
  }

  {
    std::istringstream in("P 1.5, 2.5, 3.5]");
    Point3d v(0., 0., 0.);
    in >> v;
    EXPECT_EQ(Point3d(0., 0., 0.), v);
  }

  {
    std::istringstream in("P[ 1.5, 2.5, 3.5");
    Point3d v(0., 0., 0.);
    in >> v;
    EXPECT_EQ(Point3d(0., 0., 0.), v);
  }

  {
    std::istringstream in("P[ 1.5 3.5]");
    Point2d v(0., 0.);
    in >> v;
    EXPECT_EQ(Point2d(0., 0.), v);
  }

  {
    std::istringstream in("P[ 1.5, 3.5 ]");
    Point2d v(0., 0.);
    in >> v;
    EXPECT_EQ(Point2d(1.5, 3.5), v);
  }
}

TEST(Vector, Product) {
  Point3d pscale3d(1.0, 2.0, 3.0);
  Vector3d vscale3d(1.0, 2.0, 3.0);
  Point3d p3 = Point3d(5.0, 6.0, 7.0) * pscale3d;
  Vector3d v3 = vscale3d * Vector3d(7.0, 6.0, 5.0);
  EXPECT_EQ(p3, Point3d(5.0, 12.0, 21.0));
  EXPECT_EQ(v3, Vector3d(7.0, 12.0, 15.0));

  Point2f pscale2f(1.0f, 3.0f);
  Vector2f vscale2f(1.0f, 3.0f);
  Point2f p2f = Point2f(5.0f, 7.0f) * pscale2f;
  Vector2f v2f = vscale2f * Vector2f(7.0f, 5.0f);
  EXPECT_EQ(p2f, Point2f(5.0f, 21.0f));
  EXPECT_EQ(v2f, Vector2f(7.0f, 15.0f));

  Point2i pscale2i(1, 3);
  Vector2i vscale2i(1, 3);
  Point2i p2i = Point2i(5, 7) * pscale2i;
  Vector2i v2i = vscale2i * Vector2i(7, 5);
  EXPECT_EQ(p2i, Point2i(5, 21));
  EXPECT_EQ(v2i, Vector2i(7, 15));
}

TEST(Vector, Quotient) {
  Point3d pscale3d(1.0, 2.0, 3.0);
  Vector3d vscale3d(1.0, 2.0, 3.0);
  Point3d p3 = Point3d(5.0, 4.0, 6.0) / pscale3d;
  Vector3d v3 = vscale3d / Vector3d(2.0, 4.0, 6.0);
  EXPECT_EQ(p3, Point3d(5.0, 2.0, 2.0));
  EXPECT_EQ(v3, Vector3d(0.5, 0.5, 0.5));

  Point2i pscale2i(12, 15);
  Vector2i vscale2i(12, 15);
  Point2i p2i = Point2i(24, 45) / pscale2i;
  Vector2i v2i = vscale2i / Vector2i(4, 3);
  EXPECT_EQ(p2i, Point2i(2, 3));
  EXPECT_EQ(v2i, Vector2i(3, 5));
}

TEST(Vector, Intrinsics) {
  //
  // Make sure that each function that uses intrinsics is called at least
  // once. Note that the intrinsics are enabled only in non-debug builds and on
  // certain platforms.
  //

  // Binary + operator.
  EXPECT_EQ(Vector2f(5.5f, 7.5f),
            Vector2f(1.5f, 2.f) + Vector2f(4.f, 5.5f));
  EXPECT_EQ(Vector2d(5.5, 7.5),
            Vector2d(1.5, 2.0) + Vector2d(4.0, 5.5));
  EXPECT_EQ(Vector3f(5.5f, 7.5f, 10.f),
            Vector3f(1.5f, 2.f, 6.5f) + Vector3f(4.f, 5.5f, 3.5f));
  EXPECT_EQ(Vector3d(5.5, 7.5, 10.0),
            Vector3d(1.5, 2.0, 6.5) + Vector3d(4.0, 5.5, 3.5));
  EXPECT_EQ(Vector4f(5.5f, 7.5f, 10.f, 3.f),
            Vector4f(1.5f, 2.f, 6.5f, -4.0f) + Vector4f(4.f, 5.5f, 3.5f, 7.f));
  EXPECT_EQ(Vector4d(5.5, 7.5, 10.0, 3.0),
            Vector4d(1.5, 2.0, 6.5, -4.0) + Vector4d(4.0, 5.5, 3.5, 7.0));

  // Binary - operator.
  EXPECT_EQ(Vector2f(-2.5f, -3.5f),
            Vector2f(1.5f, 2.f) - Vector2f(4.f, 5.5));
  EXPECT_EQ(Vector2d(-2.5, -3.5),
            Vector2d(1.5, 2.0) - Vector2d(4.0, 5.5));
  EXPECT_EQ(Vector3f(-2.5f, -3.5f, 3.f),
            Vector3f(1.5f, 2.f, 6.5f) - Vector3f(4.f, 5.5, 3.5));
  EXPECT_EQ(Vector3d(-2.5, -3.5, 3.0),
            Vector3d(1.5, 2.0, 6.5) - Vector3d(4.0, 5.5, 3.5));
  EXPECT_EQ(Vector4f(-2.5f, -3.5f, 3.f, -11.f),
            Vector4f(1.5f, 2.f, 6.5f, -4.f) - Vector4f(4.f, 5.5, 3.5, 7.f));
  EXPECT_EQ(Vector4d(-2.5, -3.5, 3.0, -11.0),
            Vector4d(1.5, 2.0, 6.5, -4.0) - Vector4d(4.0, 5.5, 3.5, 7.0));
}

TEST(Vector, VectorPointConversions) {
  Point3d p(0., 1., 2.);
  Vector3d v(0., 1., 2.);
  Point3f pf(0.f, 1.f, 2.f);
  Vector3f vf(0.f, 1.f, 2.f);
  EXPECT_EQ(p, Point3d(pf));
  EXPECT_EQ(p, Point3d(v));
  EXPECT_EQ(p, Point3d(vf));
  EXPECT_EQ(pf, Point3f(p));
  EXPECT_EQ(pf, Point3f(v));
  EXPECT_EQ(pf, Point3f(vf));
  EXPECT_EQ(v, Vector3d(p));
  EXPECT_EQ(v, Vector3d(pf));
  EXPECT_EQ(v, Vector3d(vf));
  EXPECT_EQ(vf, Vector3f(p));
  EXPECT_EQ(vf, Vector3f(pf));
  EXPECT_EQ(vf, Vector3f(v));
}

}  // namespace math
}  // namespace ion
