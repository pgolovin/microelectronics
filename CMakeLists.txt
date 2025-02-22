cmake_minimum_required(VERSION 3.10)

project(stm_simulator_project)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

execute_process(COMMAND conan profile detect --force)
execute_process(COMMAND conan install ${PROJECT_SOURCE_DIR}\\3rd_party\\conan --output-folder=${CMAKE_BINARY_DIR} --build=missing -s build_type=${CMAKE_BUILD_TYPE})
#include(${CMAKE_BINARY_DIR}/conan_toolchain.cmake)
add_compile_definitions(CMAKE_TOOLCHAIN_FILE="${CMAKE_BINARY_DIR}/conan_toolchain.cmake")

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
string(REGEX REPLACE "/W[3|4]" "/w" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")

set(VERSION_MAJOR 0)
set(VERSION_MINOR 0)

add_compile_definitions(PRODUCT_VERSION="${VERSION_MAJOR}.${VERSION_MINOR}")

# add sub-project
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/drivers)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/libraries)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/printer)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/fatfs)
add_subdirectory(${PROJECT_SOURCE_DIR}/emulator/tests)
add_subdirectory(${PROJECT_SOURCE_DIR}/emulator/tests/device_mock)
add_subdirectory(${PROJECT_SOURCE_DIR}/emulator/applications)
