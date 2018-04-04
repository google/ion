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

#ifndef ION_PORT_ANDROID_JNIUTIL_H_
#define ION_PORT_ANDROID_JNIUTIL_H_

#include <jni.h>
#include <string>

namespace ion {
namespace port {
namespace android {

// Set the pointer to the JavaVM.
//
// SetJVM(jvm);
// FindClassGlobal(env, "path/to/my/MyClass");
//
void SetJVM(JavaVM* jvm);
JavaVM* GetJVM();
jclass FindClassGlobal(JNIEnv* env, const char* class_name);
jmethodID GetStaticMethod(JNIEnv* env, jclass clazz, const char* class_name,
                          const char* name, const char* signature);
jmethodID GetMethod(JNIEnv* env, jclass clazz, const char* class_name,
                    const char* name, const char* signature);
jfieldID GetStaticMember(JNIEnv* env, jclass clazz, const char* class_name,
                         const char* name, const char* type);
jfieldID GetMember(JNIEnv* env, jclass clazz, const char* class_name,
                   const char* name, const char* type);
jfieldID GetIntMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name);
jfieldID GetLongMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name);
jfieldID GetStringMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name);
jfieldID GetBooleanMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name);
jfieldID GetIntArrayMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name);
jfieldID GetStaticIntMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name);
jfieldID GetStaticLongMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name);
jfieldID GetStaticStringMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name);
jfieldID GetStaticBooleanMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name);
jstring JavaString(JNIEnv* env, const std::string& s);
jobjectArray JavaStringArray(JNIEnv* env, int length);
jbyteArray JavaByteArray(JNIEnv* env, const std::string& bytes);
jbyteArray JavaByteArray(JNIEnv* env, const jbyte* data, jsize size);

void JavaGetByteArray(JNIEnv* env, jbyteArray array,
                      jsize first, jsize size, char* out);

// Returns the stack trace for the current pending exception and clears any
// exceptions present.
// Output is similar to JNI's ExceptionDescribe(), except that this function
// returns a string while ExceptionDescribe() directly prints to logcat.
//
// If any exception occurs while retrieving the stack trace, then a short
// message is returned explaining at what stage the failure occured, and all
// exceptions are cleared.
std::string GetExceptionStackTrace(JNIEnv* env);

// Pushes and pops a JNI local reference frame.
class LocalFrame {
 public:
  // Pushes a local reference frame frame with env->PushLocalFrame(0).
  explicit LocalFrame(JNIEnv* env);
  // Pops a local reference frame with env->PopLocalFrame(NULL);
  ~LocalFrame();
 private:
  JNIEnv* env_;
};

// Stack-allocate this to clean up a jobject at end of scope. Does not own
// the jobject, may not work if copied or moved.
class ScopedJObject {
 public:
  explicit ScopedJObject(JNIEnv* env, jobject* obj): env_(env), obj_(obj) {}
  ~ScopedJObject() { env_->DeleteLocalRef(*obj_); }
 private:
  JNIEnv* env_;
  jobject* obj_;
};

}  // namespace android
}  // namespace port
}  // namespace ion

#endif  // ION_PORT_ANDROID_JNIUTIL_H_
