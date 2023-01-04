set(HEADERS
    "printer.h"    
    "printer_entities.h"
    "printer_constants.h"
    "printer_memory_manager.h"
    "printer_gcode_driver.h"
    "printer_file_manager.h")

set(SOURCES
    "printer.c"
    "printer_memory_manager.c"
    "printer_gcode_driver.c"
    "printer_file_manager.c")

    # add sub-project
add_library(solutions STATIC ${HEADERS} ${SOURCES})

target_include_directories(solutions PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(solutions PUBLIC device_mock libraries drivers fatfs)
