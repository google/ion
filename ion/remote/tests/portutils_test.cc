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

#include "base/integral_types.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

#if defined(ION_PLATFORM_WINDOWS)
#include "Ws2tcpip.h"
#define inet_pton InetPtonA
typedef ULONG in_addr_t;
#else
#define closesocket close
#endif

namespace ion {
namespace remote {

namespace {

template <typename T>
::testing::AssertionResult AddressesAreEqual(const T& expected,
                                             const T& actual) {
  if (memcmp(&expected, &actual, sizeof(T))) {
    return ::testing::AssertionFailure()
           << "expected: " << reinterpret_cast<const uint8*>(&expected)
           << ", actual: " << reinterpret_cast<const uint8*>(&actual);
  } else {
    return ::testing::AssertionSuccess();
  }
}

const in_addr_t& GetAddressFromAddr(const sockaddr_storage& addr) {
  return reinterpret_cast<const sockaddr_in*>(&addr)->sin_addr.s_addr;
}

const in6_addr& GetAddress6FromAddr(const sockaddr_storage& addr) {
  return reinterpret_cast<const sockaddr_in6*>(&addr)->sin6_addr;
}

in_addr_t MakeAddressFromString(const char* addr) {
  in_addr_t out_addr;
  inet_pton(AF_INET, addr, &out_addr);
  return out_addr;
}

in6_addr MakeAddress6FromString(const char* addr) {
  in6_addr out_addr;
  inet_pton(AF_INET6, addr, &out_addr);
  return out_addr;
}


}  // anonymous namespace

TEST(PortutilsTest, MakeAddrs) {
  sockaddr_storage addr = MakeWildcard(AF_INET, 500);
  EXPECT_EQ(AF_INET, addr.ss_family);
  EXPECT_TRUE(AddressesAreEqual(htonl(INADDR_ANY), GetAddressFromAddr(addr)));
  EXPECT_EQ(500, GetPort(addr));
  EXPECT_EQ(sizeof(sockaddr_in), GetSockaddrLength(addr));

  addr = MakeWildcard(AF_INET6, 256);
  EXPECT_EQ(AF_INET6, addr.ss_family);
  EXPECT_TRUE(AddressesAreEqual(in6addr_any, GetAddress6FromAddr(addr)));
  EXPECT_EQ(256, GetPort(addr));
  EXPECT_EQ(sizeof(sockaddr_in6), GetSockaddrLength(addr));

  addr = MakeLoopback(AF_INET, 1);
  EXPECT_EQ(AF_INET, addr.ss_family);
  EXPECT_TRUE(
      AddressesAreEqual(htonl(INADDR_LOOPBACK), GetAddressFromAddr(addr)));
  EXPECT_EQ(1, GetPort(addr));
  EXPECT_EQ(sizeof(sockaddr_in), GetSockaddrLength(addr));

  addr = MakeLoopback(AF_INET6, 54134);
  EXPECT_EQ(AF_INET6, addr.ss_family);
  EXPECT_TRUE(AddressesAreEqual(in6addr_loopback, GetAddress6FromAddr(addr)));
  EXPECT_EQ(54134, GetPort(addr));
  EXPECT_EQ(sizeof(sockaddr_in6), GetSockaddrLength(addr));

  static constexpr char kIpv4Address[] = "1.2.3.4";
  addr = MakeAddress(AF_INET, 1234, kIpv4Address);
  EXPECT_EQ(AF_INET, addr.ss_family);
  EXPECT_TRUE(AddressesAreEqual(MakeAddressFromString(kIpv4Address),
                                GetAddressFromAddr(addr)));
  EXPECT_EQ(1234, GetPort(addr));
  EXPECT_EQ(sizeof(sockaddr_in), GetSockaddrLength(addr));

  static constexpr char kIpv6Address[] =
      "2001:0db8:85a3:dead:beef:8a2e:0370:7334";
  addr = MakeAddress(AF_INET6, 123, kIpv6Address);
  EXPECT_EQ(AF_INET6, addr.ss_family);
  EXPECT_TRUE(AddressesAreEqual(MakeAddress6FromString(kIpv6Address),
                                GetAddress6FromAddr(addr)));
  EXPECT_EQ(123, GetPort(addr));
  EXPECT_EQ(sizeof(sockaddr_in6), GetSockaddrLength(addr));
}

TEST(PortutilsTest, GetSocket) {
  sockaddr_storage addr;
  int fd = GetSocket(SOCK_DGRAM, &addr);
  if (fd > 0) {
    if (addr.ss_family == AF_INET6) {
      EXPECT_TRUE(AddressesAreEqual(in6addr_any, GetAddress6FromAddr(addr)));
      EXPECT_EQ(0, GetPort(addr));
      EXPECT_EQ(sizeof(sockaddr_in6), GetSockaddrLength(addr));
    } else {
      EXPECT_TRUE(
          AddressesAreEqual(htonl(INADDR_ANY), GetAddressFromAddr(addr)));
      EXPECT_EQ(0, GetPort(addr));
      EXPECT_EQ(sizeof(sockaddr_in), GetSockaddrLength(addr));
    }
    closesocket(fd);
  }
}

}  // namespace remote
}  // namespace ion
