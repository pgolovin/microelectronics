#enable conan package manager

find_package(gtest)
# set the project name
set(SOURCES
    "tests.cpp"
    "drivers/led.cpp"
    "drivers/pulse_engine.cpp"
    "drivers/spibus.cpp"
    "drivers/equalizer.cpp"
    "device/device.cpp")
# add sub-project
add_executable(driver_tests ${SOURCES})

#target_include_directories(driver_tests PRIVATE ${GTest_INCLUDE_DIRS})
target_link_libraries(driver_tests PUBLIC device_mock drivers GTest::gtest)

add_test(NAME drivers 
         COMMAND driver_tests.exe)
