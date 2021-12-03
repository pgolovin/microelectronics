
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

set(CMAKE_C_FLAGS_DEBUG_INIT "-Wall")
set(CMAKE_C_FLAGS_RELEASE_INIT "-Wall")

target_include_directories(drivers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(drivers PRIVATE device_mock)


