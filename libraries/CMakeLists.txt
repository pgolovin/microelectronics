
set(HEADERS
    "include/gcode.h"
    "include/user_interface.h")

set(SOURCES
    "sources/gcode.c"
    "sources/user_interface.c")

    # add sub-project
add_library(libraries STATIC ${HEADERS} ${SOURCES})

target_include_directories(libraries PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(libraries PUBLIC device_mock drivers)

