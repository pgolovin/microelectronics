#enable conan package manager

find_package(GTest)
# set the project name
set(SOURCES
    "tests.cpp"
    "drivers/led.cpp"
    "drivers/pulse_engine.cpp"
    "drivers/spibus.cpp"
    "drivers/equalizer.cpp"
    "drivers/motor.cpp"
    "drivers/termal_regulator.cpp"
    "device/device.cpp"
    "device/sdcard.cpp"
    "device/file_system.cpp"
    "libraries/gcode.cpp"
    "libraries/user_interface.cpp"
    "solutions/printer.cpp"
    "solutions/printer_memory_manager.cpp"
    "solutions/gcode_driver.cpp"
    "solutions/gcode_driver_acceleration.cpp"
    "solutions/configuration_commands.cpp"
    "solutions/printer_file_manager.cpp"
    "solutions/printer_emulator.h"
    "solutions/printer_emulator.cpp")
# add sub-project
add_executable(driver_tests ${SOURCES})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/model.gcode" "${CMAKE_BINARY_DIR}/bin/model.gcode" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/model2.gcode" "${CMAKE_BINARY_DIR}/bin/model2.gcode" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/wanhao.gcode" "${CMAKE_BINARY_DIR}/bin/wanhao.gcode" COPYONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/pla.mtl" "${CMAKE_BINARY_DIR}/bin/pla.mtl" COPYONLY)

target_link_libraries(driver_tests PUBLIC device_mock drivers libraries solutions fatfs GTest::gtest)
target_include_directories(driver_tests PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME drivers 
         COMMAND driver_tests.exe)
