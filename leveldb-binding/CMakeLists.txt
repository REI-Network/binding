# Declare the location of the source files
file(GLOB BINDING_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

# This line will tell CMake that we're building a shared library
# from the above source files
# named after the project's name
add_library(leveldb-binding SHARED ${BINDING_SOURCE_FILES})
target_include_directories(leveldb-binding PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../napi-macros ${CMAKE_JS_INC})
target_link_libraries(leveldb-binding PUBLIC leveldb ${CMAKE_JS_LIB})

# This line will give our library file a .node extension without any "lib" prefix
set_target_properties(leveldb-binding PROPERTIES PREFIX "" SUFFIX ".node")