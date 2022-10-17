cmake_minimum_required(VERSION 3.9.3)

set(module_dir ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
if(EXISTS ${module_dir})
    list(APPEND CMAKE_MODULE_PATH ${module_dir})
endif()

include(HunterGate)

HunterGate(
    URL "https://github.com/cpp-pm/hunter/archive/v0.24.7.tar.gz"
    SHA1 "908ddcb223f25fa155315baae64ce0ab48a38bf7"
	LOCAL
)

project(binding)

include(CheckCXXCompilerFlag)

CHECK_CXX_COMPILER_FLAG(-std=c++17 LIBCXX_HAS_STDCXX17_FLAG)

if(LIBCXX_HAS_STDCXX17_FLAG)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -O3")
else()
	message(FATAL_ERROR "c++17 is required")
endif()

if (NOT MSVC)
	if(NOT APPLE)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
	endif()
	if (APPLE)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -stdlib=libc++")
		set(CMAKE_OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}")
	endif()
endif(NOT MSVC)

# Build static libs
set(BUILD_SHARED_LIBS 			OFF CACHE BOOL "")

# Ignore tests and benchmarks
set(EVMONE_TESTING				OFF CACHE BOOL "")
set(LEVELDB_BUILD_TESTS			OFF CACHE BOOL "")
set(LEVELDB_BUILD_BENCHMARKS	OFF CACHE BOOL "")
set(LEVELDB_INSTALL				OFF CACHE BOOL "")
set(SNAPPY_BUILD_TESTS			OFF CACHE BOOL "")
set(SNAPPY_BUILD_BENCHMARKS		OFF CACHE BOOL "")
set(SNAPPY_INSTALL				OFF CACHE BOOL "")

# Explicitly tell leveldb that we have snappy
set(HAVE_SNAPPY 				ON)

# Use static boost libs
set(Boost_USE_STATIC_LIBS		ON)
set(Boost_USE_DEBUG_LIBS        OFF)
set(Boost_USE_RELEASE_LIBS      ON)
set(Boost_USE_MULTITHREADED 	ON)
set(Boost_USE_STATIC_RUNTIME	OFF)

hunter_add_package(Boost COMPONENTS filesystem system thread context log)
find_package(Boost CONFIG REQUIRED filesystem system thread context log)

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

# Global include path for all libs
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/libaleth")

add_subdirectory(evmone)
add_subdirectory(snappy)
add_subdirectory(leveldb)
add_subdirectory(leveldb-binding)
add_subdirectory(libaleth)
add_subdirectory(evm-binding)