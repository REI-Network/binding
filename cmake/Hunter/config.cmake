# cryptopp has very bad CMakeLists.txt config.
# We have to enforce "cross compiling mode" there by setting CMAKE_SYSTEM_VERSION=NO
# to any "false" value.
hunter_config(
    cryptopp
    VERSION
    ${HUNTER_cryptopp_VERSION} 
    CMAKE_ARGS 
    CMAKE_SYSTEM_VERSION=NO
    CMAKE_C_FLAGS=${CMAKE_C_FLAGS}
    CMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
)

hunter_config(
    libjson-rpc-cpp
    VERSION ${HUNTER_libjson-rpc-cpp_VERSION}
    CMAKE_ARGS
    UNIX_DOMAIN_SOCKET_SERVER=NO
    UNIX_DOMAIN_SOCKET_CLIENT=NO
    FILE_DESCRIPTOR_SERVER=NO
    FILE_DESCRIPTOR_CLIENT=NO
    TCP_SOCKET_SERVER=NO
    TCP_SOCKET_CLIENT=NO
    HTTP_SERVER=NO
    HTTP_CLIENT=NO
)

hunter_config(Boost VERSION 1.78.0)

if (MSVC)
    hunter_config(
        libscrypt
        VERSION ${HUNTER_libscrypt_VERSION}
        CMAKE_ARGS CMAKE_C_FLAGS=-D_CRT_SECURE_NO_WARNINGS
    )
endif()