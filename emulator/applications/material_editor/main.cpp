// material editor and compiler. creates mtl file that can be used to add custom settings for printer
#include "printer_constants.h"
#include "printer_entities.h"

#include <iostream>
#include <string>
#include <algorithm>

int main(int argc, char** argv)
{
    std::cout << "Material editor v0.0.0\n";
    std::string material_name;
    MaterialFile material = { 0 };

    bool valid = true;

    std::cout << "Enter material parameters.\n";
    do 
    {
        valid = true;
        std::cout << "Material name(8 symbols max): ";
        std::cin >> material_name;

        if (material_name.size() > 8 || !material_name.size())
        {
            std::cout << "invalid material name length: " << material_name.size() << "\n";
            valid = false;
            continue;
        }

        for (auto c : material_name)
        {
            if (!std::isalnum(c))
            {
                std::cout << "invalid symbol " << c << " in material name, only alpha numeric characters are allowed\n";
                valid = false;
                break;
            }
        }
    }
    while (!valid);

    std::cout << "Nozzle temperature in celsius: "; 
    std::cin >> material.temperature[TERMO_NOZZLE];
    std::cout << "Table temperature in celsius: ";
    std::cin >> material.temperature[TERMO_TABLE];
    std::cout << "E flow in percents (default is 100): ";
    std::cin >> material.e_flow_percent;
    if (material.e_flow_percent == 0)
    {
        material.e_flow_percent = 100;
    }
    std::cout << "Cooler power [0-255]: ";
    std::cin >> material.cooler_power;

    FILE* f = nullptr;
    fopen_s(&f, (material_name + ".mtl").c_str(), "w");
    if (!f)
    {
        std::cout << "File "<< material_name << ".mtl cannot be created, check permissions of the target directory.\n";
        return 1;
    }

    for (size_t i = 0; i < 8; ++i)
    {
        material.name[i] = std::toupper(material_name[i]);
    }
    material.security_code = MATERIAL_SEC_CODE;
    fwrite(&material, sizeof(material), 1, f);

    fclose(f);

    std::cout << "File: " << material_name << ".mtl successfully created\n";

    return 0;
}
