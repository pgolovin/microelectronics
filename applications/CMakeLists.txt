
#set(HEADERS
#)

set(MTL_EDITOR_SOURCES
    "material_editor/main.cpp")

set(CMD_COMPILER_SOURCES
    "command_compiler/main.cpp")
    # add sub-project
add_executable(MaterialEditor ${MTL_EDITOR_SOURCES})
target_link_libraries(MaterialEditor PUBLIC solutions)

add_executable(CommandCompiler ${CMD_COMPILER_SOURCES})
target_link_libraries(CommandCompiler PUBLIC libraries solutions device_mock)
