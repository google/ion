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

// This file stubs out functions that are undefined by NaCl to allow Ion
// to compile.

#include <sys/socket.h>

int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res) {
  return 0;
}

void freeaddrinfo(struct addrinfo *res) {}

char *gai_strerror(int errcode) {
  return nullptr;
}

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
  return 0;
}

void flockfile(FILE *filehandle) {}

void funlockfile(FILE *filehandle) {}
