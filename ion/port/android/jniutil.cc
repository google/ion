/**
Copyright 2016 Google Inc. All Rights Reserved.

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

#include "ion/port/android/jniutil.h"

#include <memory>

#include "ion/port/logging.h"

namespace ion {
namespace port {
namespace android {

JavaVM* s_jvm = NULL;

JavaVM* GetJVM() {
  return s_jvm;
}

void SetJVM(JavaVM* jvm) {
  s_jvm = jvm;
}

// This is not the same object that ion/base/logging.cc uses, but so long as
// LogEntryWriters are stateless this shouldn't matter. Doing it this way
// works around the fact that ionport can't depend on ionbase.
static const std::unique_ptr<LogEntryWriter> log_writer(
    CreateDefaultLogEntryWriter());

jclass FindClassGlobal(JNIEnv* env, const char* class_name) {
  jclass clazz = env->FindClass(class_name);
  jthrowable mException = env->ExceptionOccurred();
  if (mException) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    log_writer->Write(ERROR,
        std::string("Android JNI: Class ").
             append(class_name).append(" not found.").c_str());
    return NULL;
  }
  if (!clazz) {
    log_writer->Write(ERROR,
        std::string("Android JNI: Class ").
            append(class_name).append(" not found.").c_str());
    return NULL;
  }
  jclass global_clazz = static_cast<jclass>(env->NewGlobalRef(clazz));
  env->DeleteLocalRef(clazz);
  return global_clazz;
}

jmethodID GetStaticMethod(JNIEnv* env, jclass clazz, const char* class_name,
                          const char* name, const char* signature) {
  jmethodID method = env->GetStaticMethodID(clazz, name, signature);
  if (env->ExceptionCheck() || !method) {
    log_writer->Write(ERROR,
        std::string("Android JNI: static method ").
            append(name).append(" not found in class.").
            append(class_name).c_str());
    env->ExceptionClear();
    return NULL;
  }
  return method;
}

jmethodID GetMethod(JNIEnv* env, jclass clazz, const char* class_name,
                    const char* name, const char* signature) {
  jmethodID method = env->GetMethodID(clazz, name, signature);
  if (env->ExceptionCheck() || !method) {
    log_writer->Write(ERROR,
        std::string("Android JNI: method ").
            append(name).append(" not found in class ").
            append(class_name).c_str());
    env->ExceptionClear();
    return NULL;
  }
  return method;
}

jfieldID GetStaticStringMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name) {
  return GetStaticMember(env, clazz, class_name, name, "Ljava/lang/String;");
}

jfieldID GetStaticIntMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name) {
  return GetStaticMember(env, clazz, class_name, name, "I");
}

jfieldID GetStaticLongMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name) {
  return GetStaticMember(env, clazz, class_name, name, "J");
}

jfieldID GetStaticBooleanMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name) {
  return GetStaticMember(env, clazz, class_name, name, "Z");
}

jfieldID GetIntMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name) {
  return GetMember(env, clazz, class_name, name, "I");
}

jfieldID GetLongMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name) {
  return GetMember(env, clazz, class_name, name, "J");
}

jfieldID GetBooleanMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name) {
  return GetMember(env, clazz, class_name, name, "Z");
}

jfieldID GetStringMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name) {
  return GetMember(env, clazz, class_name, name, "Ljava/lang/String;");
}

jfieldID GetIntArrayMember(
    JNIEnv* env, jclass clazz, const char* class_name, const char* name) {
  return GetMember(env, clazz, class_name, name, "[I");
}

jfieldID GetStaticMember(JNIEnv* env, jclass clazz, const char* class_name,
                         const char* name, const char* type) {
  jfieldID field = env->GetStaticFieldID(clazz, name, type);
  if (env->ExceptionCheck() || !field) {
    log_writer->Write(ERROR,
        std::string("Android JNI: static field ").
            append(name).append(" not found in class ").
            append(class_name).c_str());
    env->ExceptionClear();
    return NULL;
  }
  return field;
}

jfieldID GetMember(JNIEnv* env, jclass clazz, const char* class_name,
                   const char* name, const char* type) {
  jfieldID field = env->GetFieldID(clazz, name, type);
  if (env->ExceptionCheck() || !field) {
    log_writer->Write(ERROR,
        std::string("Android JNI: field ").
            append(name).append(" not found in class ").
            append(class_name).c_str());
    env->ExceptionClear();
    return NULL;
  }
  return field;
}

jobjectArray JavaStringArray(JNIEnv* env, int length) {
  return env->NewObjectArray(length, env->FindClass("java/lang/String"),
                             env->NewStringUTF(""));
}

jstring JavaString(JNIEnv* env, const std::string& s) {
  return env->NewStringUTF(s.c_str());
}

jbyteArray JavaByteArray(JNIEnv* env, const std::string& bytes) {
  return JavaByteArray(env, reinterpret_cast<const jbyte*>(bytes.c_str()),
                       static_cast<jsize>(bytes.size()));
}

jbyteArray JavaByteArray(JNIEnv* env, const jbyte* data, jsize size) {
  if (size == 0) {
    return NULL;
  }
  jbyteArray j_array = env->NewByteArray(size);
  env->SetByteArrayRegion(j_array, 0, size, data);
  return j_array;
}

void JavaGetByteArray(JNIEnv* env, jbyteArray array,
                      jsize first, jsize size, char* out) {
  if (array == NULL) {
    return;
  }

  if (!out) {
    log_writer->Write(ERROR,
        "Android JNI: JavaGetByteArray called without a valid out.");
    return;
  }

  env->GetByteArrayRegion(array, first, size,
                          reinterpret_cast<jbyte*>(out));
}

LocalFrame::LocalFrame(JNIEnv* env) : env_(env) {
  jint result = env_->PushLocalFrame(0);
  if (result != JNI_OK) {
    log_writer->Write(ERROR,
        "Android JNI: Error on PushLocalFrame");
  }
}

LocalFrame::~LocalFrame() {
  env_->PopLocalFrame(NULL);
}

}  // namespace android
}  // namespace port
}  // namespace ion
