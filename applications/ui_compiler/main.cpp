// material editor and compiler. creates mtl file that can be used to add custom settings for printer
#include "device_mock.h"
#include "include\gcode.h"
#include "printer\printer_constants.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>

int main(int argc, char** argv)
{
    if (1 == argc)
    {
        std::cout << "UI compiler 0.0.0" << std::endl;
        std::cout << "Usage: " << std::endl <<
                     "    UICompiler.exe [options] <uimarkap.json>" << std::endl;
        return 0;
    }
    std::string file_name = argv[argc - 1];
    std::ifstream json_file(file_name);
    if (!json_file.is_open())
    {
        std::cout << "Failed to open file: " << file_name << " no such file or directory!" << std::endl;
        return 1;
    }

    nlohmann::json parser;

    try
    {
        parser = nlohmann::json::parse(json_file);
    }
    catch (nlohmann::json::parse_error& error)
    {
        std::cout << file_name << " is not a valid JSON file!" << std::endl
            << error.what() << std::endl;
        return 2;
    }

    return 0;
}
