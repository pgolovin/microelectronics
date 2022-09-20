
set(HEADERS
    "include/led.h"
    "include/sdcard.h"
    "include/display.h"
    "include/spibus.h"
    "include/pulse_engine.h"
    "include/equalizer.h"
    "include/motor.h"
    "include/termal_regulator.h")

set(SOURCES
    "sources/led.c"
    #"sources/sdcard.c" implementation of SDCARD is mocked in device_mock library
    "sources/display.c"
    "sources/spibus.c"
    "sources/pulse_engine.c"
    "sources/equalizer.c"
    "sources/motor.c"
    "sources/termal_regulator.c")

    # add sub-project
add_library(drivers STATIC ${HEADERS} ${SOURCES})

target_include_directories(drivers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(drivers PUBLIC device_mock)


