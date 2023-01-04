// material editor and compiler. creates mtl file that can be used to add custom settings for printer
#include "device_mock.h"
#include "include\gcode.h"
#include "printer_constants.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>

std::string GetError(GCODE_ERROR error)
{
    switch (error)
    {
    case GCODE_ERROR_INVALID_PARAM: return "Invalid parameter";
    case GCODE_ERROR_UNKNOWN_COMMAND: return "Invalid command";
    case GCODE_ERROR_UNKNOWN_PARAM: return "Unknown parameter name";
    }
    return "Unknown";
}

int main(int argc, char** argv)
{
    std::cout << "Commands Compiler v0.0.0\n";
    std::string command;
    std::vector<std::string> commands;

    bool complete = false;
    DeviceSettings ds;
    Device device(ds);
    AttachDevice(device);

    HGCODE code = GC_Configure(&axis_configuration, MAX_FETCH_SPEED);

    std::cout << "Enter commands (\'q\' for exit).\n";
    uint32_t i = 0;
    while (!complete)
    {
        std::cout << ++i << ": ";
        std::getline(std::cin, command);
        if (command == "q" || command == "exit" || command == "quit")
        {
            break;
        }
        GCODE_ERROR err = GC_ParseCommand(code, command.c_str());
        if (GCODE_OK != err)
        {
            std::cout << "Invalid command. Parser error: " << GetError(err) << "\n";
            continue;
        }
        commands.push_back(command);
    }

    if (!commands.size())
    {
        std::cout << "No commands provided. Aborting\n";
        return 1;
    }
    
    std::vector<uint8_t> buffer(GCODE_CHUNK_SIZE * commands.size());
    uint8_t* caret = buffer.data();

    for (const auto& cmd : commands)
    {
        GC_ParseCommand(code, cmd.c_str());
        caret += GC_CompressCommand(code, caret);
    }

    std::cout << "Enter data array name: ";
    std::cin >> command;

    std::cout << "const uint8_t " << command << "[" << buffer.size() << "] = {";
    for (size_t i = 0; i < buffer.size(); ++i)
    {
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << (uint32_t)buffer[i];
        if (i < buffer.size() - 1)
        {
            std::cout << ", ";
        }
    }
    std::cout << "};\n";

    return 0;
}
