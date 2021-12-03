
set(HEADERS
    "include/pwm.h"
    "include/sdcard.h"
    "include/spibus.h")

set(SOURCES
    "sources/pwm.c"
    "sources/sdcard.c"
    "sources/spibus.c")

    # add sub-project
add_library(drivers STATIC ${HEADERS} ${SOURCES})

target_include_directories(drivers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(drivers PUBLIC device_mock)


