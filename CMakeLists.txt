cmake_minimum_required(VERSION 3.1)

if (NOT MSVC)
	if(NOT APPLE)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
	endif()
	if (APPLE)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
		set(CMAKE_OSX_ARCHITECTURES "${CMAKE_HOST_SYSTEM_PROCESSOR}")
	endif()
endif(NOT MSVC)

set(BUILD_SHARED_LIBS 		OFF)

add_subdirectory(evmone)

project(rei-binding)

include(CheckCXXCompilerFlag)

CHECK_CXX_COMPILER_FLAG(-std=c++17 LIBCXX_HAS_STDCXX17_FLAG)

if(LIBCXX_HAS_STDCXX17_FLAG)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -O3")
else()
	message(FATAL_ERROR "need c++17")
endif()

# Explicitly tell leveldb that we have snappy
set(HAVE_SNAPPY 1)

add_subdirectory(snappy)
add_subdirectory(leveldb)
add_subdirectory(leveldb-binding)

set(Boost_USE_STATIC_LIBS		ON)
set(Boost_USE_STATIC_RUNTIME	ON)
set(BOOST_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boost)

add_definitions(-DBOOST_LOCALE_HIDE_AUTO_PTR)
add_definitions(-DBOOST_COROUTINES_NO_DEPRECATION_WARNING)

add_subdirectory(boost)

# Global include path for all libs.
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/libaleth")

add_subdirectory(libaleth)
add_subdirectory(evm-binding)