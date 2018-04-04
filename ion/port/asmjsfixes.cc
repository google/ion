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

// This file stubs out functions that are undefined by Emscripten to allow Ion
// to compile.

#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>

int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res) {
  return 0;
}

void freeaddrinfo(struct addrinfo *res) {}

const char *gai_strerror(int errcode) {
  return nullptr;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate) {
  return 0;
}

int pthread_create(pthread_t *thread, const pthread_attr_t* attr,
                   void *(*start_routine)(void* arg), void* arg) {
  assert(false && "Asmjs can't create threads!");
  return 0;
}

int pthread_cond_signal(pthread_cond_t *cond) {
  return 0;
}

int pthread_join(pthread_t thread, void **retval) {
  return 0;
}
