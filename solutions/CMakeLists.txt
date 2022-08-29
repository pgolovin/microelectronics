
set(HEADERS
    "printer/printer_entities.h"
    "printer/printer_gcode_driver.h")

set(SOURCES
    "printer/printer_gcode_driver.c")

    # add sub-project
add_library(solutions STATIC ${HEADERS} ${SOURCES})

target_include_directories(solutions PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(solutions PUBLIC device_mock libraries drivers)
