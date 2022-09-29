// material editor and compiler. creates mtl file that can be used to add custom settings for printer
#include <iostream>
#include <string>
#include <algorithm>

int main(int argc, char** argv)
{
    std::cout << "Material editor v0.0.0\n";
    std::string material_name;
    uint16_t nozzle_temperature = 0;
    uint16_t table_temperature = 0;
    uint16_t e_flow_percent = 100;
    uint16_t cooler_power = 100;

    bool valid = true;

    std::cout << "Enter meterial parameters.\n";
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
    std::cin >> nozzle_temperature;
    std::cout << "Table temperature in celsius: ";
    std::cin >> table_temperature;
    std::cout << "E flow in percents (default is 100): ";
    std::cin >> e_flow_percent;
    if (e_flow_percent == 0)
    {
        e_flow_percent = 100;
    }
    std::cout << "Cooler power [0-255]: ";
    std::cin >> cooler_power;

    FILE* f = nullptr;
    fopen_s(&f, (material_name + ".mtl").c_str(), "w");
    if (!f)
    {
        std::cout << "File "<< material_name << ".mtl cannot be created, check permissions of the target directory.\n";
        return 1;
    }

    char name[9] = {0};
    for (size_t i = 0; i < 8; ++i)
    {
        name[i] = std::toupper(material_name[i]);
    }

    fwrite(name, sizeof(char),  sizeof(name), f);
    fwrite(&nozzle_temperature, sizeof(nozzle_temperature), 1, f);
    fwrite(&table_temperature,  sizeof(table_temperature), 1, f);
    fwrite(&e_flow_percent,     sizeof(e_flow_percent), 1, f);
    fwrite(&cooler_power,       sizeof(cooler_power), 1, f);

    fclose(f);

    std::cout << "File: " << material_name << ".mtl successfully created\n";

    return 0;
}
