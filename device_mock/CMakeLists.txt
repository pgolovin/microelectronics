# set the project name
set(HEADERS
    "include/main.h"
    "include/stm32f7xx_hal_spi.h")

set(SOURCES
    "src/main.cpp"
    "src/stm32f7xx_hal_spi.cpp")

add_library(device_mock STATIC ${HEADERS} ${SOURCES})

target_precompile_headers(device_mock PUBLIC include/main.h)
target_include_directories(device_mock PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# add sub-project

