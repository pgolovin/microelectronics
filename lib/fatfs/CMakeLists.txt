
set(HEADERS
    "include/ffconf.h"
    "include/ff.h"
    "include/diskio.h")

set(SOURCES
    "sources/ff.c"
    "sources/diskio.c"
    "sources/ffsystem.c"
    "sources/ffunicode.c")

    # add sub-project
add_library(fatfs STATIC ${HEADERS} ${SOURCES})

target_include_directories(fatfs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(fatfs PUBLIC device_mock)


