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

#ifndef ION_REMOTE_PORTUTILS_H_
#define ION_REMOTE_PORTUTILS_H_

#if defined(ION_PLATFORM_WINDOWS)
#include <winsock2.h>  // NOLINT
#include <ws2ipdef.h>
typedef int socklen_t;
typedef SOCKET ion_socket_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int ion_socket_t;
#endif

namespace ion {
namespace remote {

// Returns a wildcard sockaddr_storage structure for the given family and port.
sockaddr_storage MakeWildcard(int family, int port);

// Returns a loopback sockaddr_storage structure for the given family and port.
sockaddr_storage MakeLoopback(int family, int port);

// Returns a sockaddr_storage structure for the given family and port with the
// passed address.
sockaddr_storage MakeAddress(int family, int port, const char* address);

// Obtains either an ipv6 or ipv4 socket for use with the passed |protocol|.
// The family (AF_INET or AF_INET6) is returned in |family_out|.
int GetSocket(int protocol, sockaddr_storage* addr);

// Retrieves the port from the passed address.
int GetPort(const sockaddr_storage& addr);

// Returns the length the passed storage depending on its family.
socklen_t GetSockaddrLength(const sockaddr_storage& addr);

}  // namespace remote
}  // namespace ion

#endif  // ION_REMOTE_PORTUTILS_H_
