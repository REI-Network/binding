# Declare the location of the source files
file(GLOB BINDING_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# This line will tell CMake that we're building a shared library
# from the above source files
# named after the project's name
add_library(evm-binding SHARED ${BINDING_SOURCE_FILES})

# This line will give our library file a .node extension without any "lib" prefix
set_target_properties(evm-binding PROPERTIES PREFIX "" SUFFIX ".node")

# Include Node-API wrappers
execute_process(COMMAND node -p "require('node-addon-api').include"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE NODE_ADDON_API_DIR
        )
string(REGEX REPLACE "[\r\n\"]" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})
target_include_directories(evm-binding PRIVATE ${CMAKE_JS_INC} ${NODE_ADDON_API_DIR})

# Link libraries
target_link_directories(evm-binding PUBLIC ${JSONCPP_ROOT}/lib)
target_link_libraries(evm-binding PUBLIC devcore devcrypto ethashseal ethcore ethereum evm ${CMAKE_JS_LIB})

# define NAPI_VERSION
add_definitions(-DNAPI_VERSION=6)