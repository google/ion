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

#include "ion/demos/demobase.h"

#include <jni.h>

#include "ion/base/stringutils.h"

#define LOGI(...)


//-----------------------------------------------------------------------------
//
// Java interface functions.
//
//-----------------------------------------------------------------------------

#define JNI_EXPORT extern "C" __attribute__((visibility("default")))

static DemoBase* GetDemo(jlong jni_pointer) {
  return reinterpret_cast<DemoBase*>(jni_pointer);
}

JNI_EXPORT jlong Java___jni_name___IonRenderer_nativeInit(
    JNIEnv* env, jobject thiz, jlong demo, jint w, jint h) {
  // The lifetime of the native demo object is managed in Java.
  return reinterpret_cast<jlong>(CreateDemo(w, h).release());
}

JNI_EXPORT void Java___jni_name___IonRenderer_nativeResize(
    JNIEnv* env, jobject thiz, jlong demo, jint w, jint h) {
  GetDemo(demo)->Resize(w, h);
}

JNI_EXPORT void Java___jni_name___IonRenderer_nativeRender(
    JNIEnv* env, jobject thiz, jlong demo) {
  GetDemo(demo)->Update();
  GetDemo(demo)->Render();
}

JNI_EXPORT void Java___jni_name___IonRenderer_nativeMotion(
    JNIEnv* env, jobject thiz, jlong demo, jfloat x, jfloat y,
    jboolean is_press) {
  GetDemo(demo)->ProcessMotion(x, y, is_press);
}

JNI_EXPORT void Java___jni_name___IonRenderer_nativeScale(
    JNIEnv* env, jobject thiz, jlong demo, jfloat scale) {
  GetDemo(demo)->ProcessScale(scale);
}

JNI_EXPORT void Java___jni_name___IonRenderer_nativeDone(
    JNIEnv* env, jobject thiz, jlong demo) {
  delete GetDemo(demo);
}
