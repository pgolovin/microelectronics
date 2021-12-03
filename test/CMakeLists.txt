#enable conan package manager

find_package(gtest)
# set the project name
set(SOURCES
    "tests.cpp"
    "drivers/pmw.cpp")
# add sub-project
add_executable(driver_tests ${SOURCES})

target_include_directories(driver_tests PRIVATE ${GTest_INCLUDE_DIRS})
target_link_libraries(driver_tests PRIVATE device_mock drivers GTest::gtest)

add_test(NAME drivers 
         COMMAND driver_tests.exe)
