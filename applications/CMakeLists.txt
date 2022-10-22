
find_package(nlohmann_json)

set(MTL_EDITOR_SOURCES
    "material_editor/main.cpp")

set(CMD_COMPILER_SOURCES
    "command_compiler/main.cpp")

set(UI_COMPILER_SOURCES
    "ui_compiler/main.cpp")
    # add sub-project
add_executable(MaterialEditor ${MTL_EDITOR_SOURCES})
target_link_libraries(MaterialEditor PUBLIC solutions)

add_executable(CommandCompiler ${CMD_COMPILER_SOURCES})
target_link_libraries(CommandCompiler PUBLIC libraries solutions device_mock)

add_executable(UICompiler ${UI_COMPILER_SOURCES})
target_link_libraries(UICompiler PUBLIC nlohmann_json::nlohmann_json libraries solutions device_mock)