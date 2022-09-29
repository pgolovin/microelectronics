
#set(HEADERS
#)

set(SOURCES
    "material_editor/main.cpp")

    # add sub-project
add_executable(MaterialEditor ${HEADERS} ${SOURCES})
target_link_libraries(MaterialEditor PUBLIC solutions)

