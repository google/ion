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

#include "ion/base/vectordatacontainer.h"

#include "ion/base/logchecker.h"
#include "ion/base/tests/testallocator.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

TEST(VectorDataContainerTest, UnwipableData) {
  VectorDataContainer<double>* vdc = new VectorDataContainer<double>(false);
  DataContainerPtr ptr(vdc);
  EXPECT_TRUE(ptr->GetData() == nullptr);
  EXPECT_FALSE(ptr->IsWipeable());

  const AllocVector<double>& data = vdc->GetVector();
  EXPECT_TRUE(data.empty());
  AllocVector<double>& mut_data = *vdc->GetMutableVector();
  EXPECT_TRUE(mut_data.empty());
  EXPECT_EQ(&mut_data, &data);

  mut_data.push_back(100.);
  mut_data.push_back(102.);
  EXPECT_FALSE(ptr->GetData() == nullptr);
  const double* data_ptr = ptr->GetData<double>();
  EXPECT_EQ(100., data_ptr[0]);
  EXPECT_EQ(102., data_ptr[1]);
  EXPECT_EQ(data_ptr, &data[0]);

  ptr->WipeData();
  EXPECT_FALSE(ptr->GetData() == nullptr);
  EXPECT_FALSE(data.empty());
  EXPECT_FALSE(mut_data.empty());
  data_ptr = ptr->GetData<double>();
  EXPECT_EQ(100., data_ptr[0]);
  EXPECT_EQ(102., data_ptr[1]);
  EXPECT_EQ(data_ptr, &data[0]);
}

TEST(VectorDataContainerTest, WipableData) {
  VectorDataContainer<int>* vdc = new VectorDataContainer<int>(true);
  DataContainerPtr ptr(vdc);
  EXPECT_TRUE(ptr->GetData() == nullptr);
  EXPECT_TRUE(ptr->IsWipeable());

  const AllocVector<int>& data = vdc->GetVector();
  EXPECT_TRUE(data.empty());
  AllocVector<int>& mut_data = *vdc->GetMutableVector();
  EXPECT_TRUE(mut_data.empty());
  EXPECT_EQ(&mut_data, &data);

  mut_data.push_back(10);
  EXPECT_FALSE(ptr->GetData() == nullptr);
  const int* data_ptr = ptr->GetData<int>();
  EXPECT_EQ(10, data_ptr[0]);
  EXPECT_EQ(data_ptr, &data[0]);

  ptr->WipeData();
  EXPECT_TRUE(ptr->GetData() == nullptr);
  EXPECT_TRUE(data.empty());
  EXPECT_TRUE(mut_data.empty());
  EXPECT_GE(1U, data.capacity());
}

}  // namespace base
}  // namespace ion
