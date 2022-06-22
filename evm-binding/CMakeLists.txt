# Declare the location of the source files
file(GLOB BINDING_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# This line will tell CMake that we're building a shared library
# from the above source files
# named after the project's name
add_library(evm-binding SHARED ${BINDING_SOURCE_FILES})
target_include_directories(evm-binding PRIVATE ${CMAKE_JS_INC})
target_link_libraries(evm-binding PUBLIC leveldb evmone evmc ${CMAKE_JS_LIB})

# This line will give our library file a .node extension without any "lib" prefix
set_target_properties(evm-binding PROPERTIES PREFIX "" SUFFIX ".node")