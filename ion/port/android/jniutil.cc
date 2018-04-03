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

#include "ion/port/android/jniutil.h"

#include <memory>

#include "ion/port/logging.h"

namespace ion {
namespace port {
namespace android {

namespace {

// Stack-allocate this to clear any pending JNI exceptions at end of scope.
class ScopedExceptionClearer {
 public:
  explicit ScopedExceptionClearer(JNIEnv* env): env_(env) {}
  ~ScopedExceptionClearer() { env_->ExceptionClear(); }
 private:
  JNIEnv* env_;
};

}  // namespace

JavaVM* s_jvm = nullptr;

JavaVM* GetJVM() {
  return s_jvm;
}

void SetJVM(JavaVM* jvm) {
  s_jvm = jvm;
}

// This is not the same object that ion/base/logging.cc uses, but so long as
// LogEntryWriters are stateless this shouldn't matter. Doing it this way
// works around the fact that ionport can't depend on ionbase.
LogEntryWriter* log_writer() {
  static LogEntryWriter* const log_writer = CreateDefaultLogEntryWriter();
  return log_writer;
}

jclass FindClassGlobal(JNIEnv* env, const char* class_name) {
  jclass clazz = env->FindClass(class_name);
  jthrowable mException = env->ExceptionOccurred();
  if (mException) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    log_writer()->Write(ERROR, std::string("Android JNI: Class ")
                                   .append(class_name)
                                   .append(" not found.")
                                   .c_str());
    return nullptr;
  }
  if (!clazz) {
    log_writer()->Write(ERROR, std::string("Android JNI: Class ")
                                   .append(class_name)
                                   .append(" not found.")
                                   .c_str());
    return nullptr;
  }
  jclass global_clazz = static_cast<jclass>(env->NewGlobalRef(clazz));
  env->DeleteLocalRef(clazz);
  return global_clazz;
}

jmethodID GetStaticMethod(JNIEnv* env, jclass clazz, const char* class_name,
                          const char* name, const char* signature) {
  jmethodID method = env->GetStaticMethodID(clazz, name, signature);
  if (env->ExceptionCheck() || !method) {
    log_writer()->Write(ERROR, std::string("Android JNI: static method ")
                                   .append(name)
                                   .append(" not found in class: ")
                                   .append(class_name)
                                   .c_str());
    env->ExceptionClear();
    return nullptr;
  }
  return method;
}

jmethodID GetMethod(JNIEnv* env, jclass clazz, const char* class_name,
                    const char* name, const char* signature) {
  jmethodID method = env->GetMethodID(clazz, name, signature);
  if (env->ExceptionCheck() || !method) {
    log_writer()->Write(ERROR, std::string("Android JNI: method ")
                                   .append(name)
                                   .append(" not found in class ")
                                   .append(class_name)
                                   .c_str());
    env->ExceptionClear();
    return nullptr;
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
    log_writer()->Write(ERROR, std::string("Android JNI: static field ")
                                   .append(name)
                                   .append(" not found in class ")
                                   .append(class_name)
                                   .c_str());
    env->ExceptionClear();
    return nullptr;
  }
  return field;
}

jfieldID GetMember(JNIEnv* env, jclass clazz, const char* class_name,
                   const char* name, const char* type) {
  jfieldID field = env->GetFieldID(clazz, name, type);
  if (env->ExceptionCheck() || !field) {
    log_writer()->Write(ERROR, std::string("Android JNI: field ")
                                   .append(name)
                                   .append(" not found in class ")
                                   .append(class_name)
                                   .c_str());
    env->ExceptionClear();
    return nullptr;
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
    return nullptr;
  }
  jbyteArray j_array = env->NewByteArray(size);
  env->SetByteArrayRegion(j_array, 0, size, data);
  return j_array;
}

void JavaGetByteArray(JNIEnv* env, jbyteArray array,
                      jsize first, jsize size, char* out) {
  if (array == nullptr) {
    return;
  }

  if (!out) {
    log_writer()->Write(
        ERROR, "Android JNI: JavaGetByteArray called without a valid out.");
    return;
  }

  env->GetByteArrayRegion(array, first, size,
                          reinterpret_cast<jbyte*>(out));
}

std::string GetExceptionStackTrace(JNIEnv* env) {
  static const std::string kDefaultExceptionString(
      "Could not get exception string.");

  // Grab the current exception then clear exceptions otherwise subsequent jni
  // methods will fail with "method XXX called with pending exception" errors.
  jthrowable exception = env->ExceptionOccurred();
  if (exception == nullptr) return "Error - no exception pending.";

  env->ExceptionClear();

  // Perform the equivalent of the following Java code:
  //
  // 1) java.io.StringWriter sw = new java.io.StringWriter();
  // 2) java.io.PrintWriter pw = new java.io.PrintWriter(sw);
  // 3) exception.printStackTrace(pw);
  // 4) return sw.toString();

  // Get necessary jclass and jmethodID objects.
  const jclass string_writer_class = env->FindClass("java/io/StringWriter");
  const jmethodID string_writer_constructor = GetMethod(env,
      string_writer_class, "java/io/StringWriter", "<init>", "()V");
  const jclass print_writer_class = env->FindClass("java/io/PrintWriter");
  const jmethodID print_writer_constructor = GetMethod(env,
      print_writer_class, "java/io/PrintWriter", "<init>",
      "(Ljava/io/Writer;)V");
  const jclass throwable_class = env->FindClass("java/lang/Throwable");
  const jmethodID throwable_print_stack_trace = GetMethod(env,
      throwable_class, "java/lang/Throwable", "printStackTrace",
      "(Ljava/io/PrintWriter;)V");
  const jclass object_class = env->FindClass("java/lang/Object");
  const jmethodID to_string_method = GetMethod(env, object_class,
      "java/lang/Object", "toString", "()Ljava/lang/String;");

  // Make sure we clear any additional exceptions we might create with the
  // jni calls below before returning.
  ScopedExceptionClearer exception_clearer(env);

  if (!(string_writer_class && string_writer_constructor &&
        print_writer_class && print_writer_constructor &&
        throwable_class && throwable_print_stack_trace &&
        object_class && to_string_method)) {
    return kDefaultExceptionString + "Error instantiating necessary jclass "
      + "or jmethodID objects.";
  }

  // 1) java.io.StringWriter sw = new java.io.StringWriter();
  jobject string_writer = env->NewObject(string_writer_class,
      string_writer_constructor);
  if (string_writer == nullptr)
    return kDefaultExceptionString + "Error instantiating StringWriter";

  // 2) java.io.PrintWriter pw = new java.io.PrintWriter(sw);
  jobject print_writer = env->NewObject(print_writer_class,
      print_writer_constructor, string_writer);
  if (print_writer == nullptr)
    return kDefaultExceptionString + "Error instantiating PrintWriter";

  // 3) exception.printStackTrace(pw);
  env->CallVoidMethod(exception, throwable_print_stack_trace, print_writer);

  // 4) return sw.toString();
  jstring java_string = static_cast<jstring>(
    env->CallObjectMethod(string_writer, to_string_method));
  if (java_string == nullptr)
    return kDefaultExceptionString + "Error calling toString()";

  // Convert jstring to std::string.
  const char* string_data = env->GetStringUTFChars(java_string, nullptr);
  size_t size = env->GetStringUTFLength(java_string);
  std::string cpp_string(string_data, size);
  env->ReleaseStringUTFChars(java_string, string_data);
  return cpp_string;
}

LocalFrame::LocalFrame(JNIEnv* env) : env_(env) {
  jint result = env_->PushLocalFrame(0);
  if (result != JNI_OK) {
    log_writer()->Write(ERROR, "Android JNI: Error on PushLocalFrame");
  }
}

LocalFrame::~LocalFrame() {
  env_->PopLocalFrame(nullptr);
}

}  // namespace android
}  // namespace port
}  // namespace ion
