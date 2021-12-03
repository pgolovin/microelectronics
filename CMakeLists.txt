cmake_minimum_required(VERSION 3.10)

project(stm_simulator_project)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

include(${PROJECT_SOURCE_DIR}/3rd_party/conan/conan.cmake)

conan_cmake_configure(REQUIRES gtest/1.11.0 
                      GENERATORS cmake_find_package)

conan_cmake_autodetect(CONAN_SETTINGS)

conan_cmake_install(PATH_OR_REFERENCE ${CMAKE_BINARY_DIR}
                    BUILD missing
                    REMOTE conancenter
                    SETTINGS ${CONAN_SETTINGS})

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_options("/WX" "/W4")

# add sub-project
add_subdirectory(${PROJECT_SOURCE_DIR}/device_mock)
add_subdirectory(${PROJECT_SOURCE_DIR}/drivers)
add_subdirectory(${PROJECT_SOURCE_DIR}/test)