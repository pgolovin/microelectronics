#enable conan package manager

find_package(gtest)
# set the project name
set(SOURCES
    "tests.cpp"
    "drivers/led.cpp"
    "drivers/pulse_engine.cpp"
    "drivers/spibus.cpp"
    "drivers/equalizer.cpp"
    "device/device.cpp"
    "libraries/gcode.cpp")
# add sub-project
add_executable(driver_tests ${SOURCES})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/resources/wanhao.gcode" "${CMAKE_BINARY_DIR}/bin/wanhao.gcode" COPYONLY)

message(STATUS "${CMAKE_BINARY_DIR}/bin/wanhao.gcode")
#target_include_directories(driver_tests PRIVATE ${GTest_INCLUDE_DIRS})
target_link_libraries(driver_tests PUBLIC device_mock drivers libraries GTest::gtest)

add_test(NAME drivers 
         COMMAND driver_tests.exe)
