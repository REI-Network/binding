# Aleth: Ethereum C++ client, tools and libraries.
# Copyright 2013-2019 Aleth Authors.
# Licensed under the GNU General Public License, Version 3. See the LICENSE file.

cmake_minimum_required(VERSION 3.9.3)

set(module_dir ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
if(EXISTS ${module_dir})
    list(APPEND CMAKE_MODULE_PATH ${module_dir})
endif()

set(UTILS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/utils")

HunterGate(
    URL "https://github.com/cpp-pm/hunter/archive/v0.24.5.tar.gz"
    SHA1 "d7279c19372938dbb14c6ed511bdcb6e938ac5df"
    LOCAL
)

hunter_add_package(jsoncpp)
find_package(jsoncpp CONFIG REQUIRED)

hunter_add_package(cryptopp)
find_package(cryptopp CONFIG REQUIRED)

hunter_add_package(libscrypt)
find_package(libscrypt CONFIG REQUIRED)

hunter_add_package(ethash)
find_package(ethash CONFIG REQUIRED)

include(ProjectSecp256k1)
include(ProjectLibFF)

find_package(Threads)

if(MINIUPNPC)
    find_package(Miniupnpc 1.8.2013 REQUIRED)
endif()

add_subdirectory(libdevcore)
add_subdirectory(libdevcrypto)
add_subdirectory(libethcore)
add_subdirectory(libevm)
add_subdirectory(libethereum)