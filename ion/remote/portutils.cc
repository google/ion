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

#include "ion/remote/portutils.h"

#include <initializer_list>

#include "base/integral_types.h"

#if defined(ION_PLATFORM_WINDOWS)
#include "ws2tcpip.h"  // NOLINT(build/include)
#define inet_pton InetPtonA
#endif

namespace ion {
namespace remote {

namespace {

enum SocketAddress {
  kAddress,
  kAny,
  kLoopback
};

sockaddr_storage MakeSockaddr(int family, int port, SocketAddress address,
                              const char* address_string) {
  sockaddr_storage addr = {};
  if (family == AF_INET6) {
    sockaddr_in6* paddr = reinterpret_cast<sockaddr_in6*>(&addr);
    paddr->sin6_family = AF_INET6;
    if (address == kAny) {
      paddr->sin6_addr = in6addr_any;
    } else if (address == kLoopback) {
      paddr->sin6_addr = in6addr_loopback;
    } else {
      inet_pton(family, address_string, &paddr->sin6_addr);
    }
    paddr->sin6_port = static_cast<uint16>(htons(static_cast<uint16>(port)));
  } else {
    sockaddr_in* paddr = reinterpret_cast<sockaddr_in*>(&addr);
    paddr->sin_family = AF_INET;
    if (address == kAny) {
      paddr->sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (address == kLoopback) {
      paddr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else {
      inet_pton(family, address_string, &paddr->sin_addr.s_addr);
    }
    paddr->sin_port = htons(static_cast<uint16>(port));
  }
  return addr;
}

}  // anonymous namespace

sockaddr_storage MakeWildcard(int family, int port) {
  return MakeSockaddr(family, port, kAny, nullptr);
}

sockaddr_storage MakeLoopback(int family, int port) {
  return MakeSockaddr(family, port, kLoopback, nullptr);
}

sockaddr_storage MakeAddress(int family, int port, const char* address) {
  return MakeSockaddr(family, port, kAddress, address);
}

int GetSocket(int protocol, sockaddr_storage* addr) {
  // Try IPV6 first.
  for (int family : {AF_INET6, AF_INET}) {
    const ion_socket_t fd = socket(family, protocol, 0);
#if defined(ION_PLATFORM_WINDOWS)
    const bool socket_is_valid = fd != INVALID_SOCKET;
#else
    const bool socket_is_valid = fd >= 0;
#endif
    if (socket_is_valid) {
      *addr = MakeWildcard(family, 0);
      return static_cast<int>(fd);
    }
  }
  return -1;
}

int GetPort(const sockaddr_storage& addr) {
  if (addr.ss_family == AF_INET6) {
    return ntohs(reinterpret_cast<const sockaddr_in6*>(&addr)->sin6_port);
  } else {
    return ntohs(reinterpret_cast<const sockaddr_in*>(&addr)->sin_port);
  }
}

socklen_t GetSockaddrLength(const sockaddr_storage& addr) {
  if (addr.ss_family == AF_INET6) {
    return sizeof(sockaddr_in6);
  } else {
    return sizeof(sockaddr_in);
  }
}

}  // namespace remote
}  // namespace ion
