# set the project name
set(HEADERS
    "include/main.h"
    "include/stm32f7xx_hal_spi.h"
    "include/device_mock.h"
    "include/sdcard.h"
    "include/sdcard_mock.h"
    "include/display.h"
    "include/display_mock.h")

set(SOURCES
    "src/main.cpp"
    "src/stm32f7xx_hal_spi.cpp"
    "src/device_mock.cpp"
    "src/sdcard_mock.cpp"
    "src/display_mock.cpp")

add_library(device_mock STATIC ${HEADERS} ${SOURCES})

target_precompile_headers(device_mock PUBLIC include/main.h)
target_include_directories(device_mock PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(device_mock PRIVATE drivers libraries)
# add sub-project

