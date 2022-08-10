
#include "include/gcode.h"
#include "device_mock.h"

#include <gtest/gtest.h>

TEST(GCodeBasicTest, cannot_create_without_config)
{
    std::unique_ptr<Device> device;
    DeviceSettings ds;
    device = std::make_unique<Device>(ds);
    AttachDevice(*device);

    HGCODE code = nullptr;
    code = GC_Configure(nullptr);
    ASSERT_TRUE(nullptr == code);

    DetachDevice();
    device = nullptr;
}

TEST(GCodeBasicTest, can_create)
{
    std::unique_ptr<Device> device;
    DeviceSettings ds;
    device = std::make_unique<Device>(ds);
    AttachDevice(*device);
    GCodeAxisConfig cfg = { 1, 1, 1, 100 };

    HGCODE code = nullptr;
    code = GC_Configure(&cfg);
    ASSERT_FALSE(nullptr == code);

    DetachDevice();
    device = nullptr;
}

class GCodeParserTest : public ::testing::Test
{
protected:
    std::unique_ptr<Device> device;
    HGCODE code = nullptr;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        GCodeAxisConfig cfg = { 1, 1, 1, 100 };
        code = GC_Configure(&cfg);
    }

    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(GCodeParserTest, command_noop)
{
    ASSERT_EQ(GC_GetCurrentCommandCode(code), GCODE_COMMAND_NOOP);
}

TEST_F(GCodeParserTest, command_noop_no_G_data)
{
    ASSERT_TRUE(nullptr == GC_GetCurrentCommand(code));
}

TEST_F(GCodeParserTest, command_noop_no_M_data)
{
    ASSERT_TRUE(nullptr == GC_GetCurrentSubCommand(code));
}

TEST_F(GCodeParserTest, command_category)
{
    std::string command = "G0 F6200 X0 Y100 Z10 E1.764";
    ASSERT_EQ((int)GCODE_OK_COMMAND_CREATED, (int)GC_ParseCommand(code, const_cast<char*>(command.c_str())));
}

TEST_F(GCodeParserTest, subcommand_category)
{
    std::string command = "M104 S240";
    ASSERT_EQ((int)GCODE_OK_COMMAND_CREATED, (int)GC_ParseCommand(code, const_cast<char*>(command.c_str())));
}

TEST_F(GCodeParserTest, comment)
{
    std::string command = ";this is the comment in gcode";
    ASSERT_EQ((int)GCODE_OK_NO_COMMAND, (int)GC_ParseCommand(code, const_cast<char*>(command.c_str())));
}

TEST_F(GCodeParserTest, comment_leads_to_noop)
{
    std::string command = ";this is the comment in gcode";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    ASSERT_EQ(GC_GetCurrentCommandCode(code), GCODE_COMMAND_NOOP);
}

TEST_F(GCodeParserTest, trailing_comment)
{
    std::string command = "M104 ;some additional comment";
    ASSERT_EQ((int)GCODE_OK_COMMAND_CREATED, (int)GC_ParseCommand(code, const_cast<char*>(command.c_str())));
}

TEST_F(GCodeParserTest, empty_line)
{
    std::string command = ";this is the comment in gcode";
    ASSERT_EQ((int)GCODE_OK_NO_COMMAND, (int)GC_ParseCommand(code, const_cast<char*>(command.c_str())));
}

TEST_F(GCodeParserTest, invalid_command)
{
    std::string command = "Q0 F6200 X0 Y100 Q10 E1.764";
    ASSERT_EQ((int)GCODE_ERROR_UNKNOWN_COMMAND, (int)GC_ParseCommand(code, const_cast<char*>(command.c_str())));
}

TEST_F(GCodeParserTest, short_command)
{
    std::string command = "G92";
    ASSERT_EQ((int)GCODE_OK_COMMAND_CREATED, (int)GC_ParseCommand(code, const_cast<char*>(command.c_str())));
}

TEST_F(GCodeParserTest, invalid_command_leads_to_noop)
{
    std::string command = "Q92";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    ASSERT_EQ((int)GCODE_COMMAND_NOOP, (int)GC_GetCurrentCommandCode(code));
}

TEST_F(GCodeParserTest, invalid_param)
{
    std::string command = "G0 F6200 X0 Y100 Q10 E1.764";
    ASSERT_EQ((int)GCODE_ERROR_UNKNOWN_PARAM, (int)GC_ParseCommand(code, const_cast<char*>(command.c_str())));
}

TEST_F(GCodeParserTest, invalid_parameter_leads_to_noop)
{
    std::string command = "G1 X1.21 Z4.32 Q33";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    ASSERT_EQ((int)GCODE_COMMAND_NOOP, (int)GC_GetCurrentCommandCode(code));
}

TEST_F(GCodeParserTest, command_M_rejects_G_params)
{
    std::string command = "M100 S200 X1.75";
    ASSERT_EQ((int)GCODE_ERROR_UNKNOWN_PARAM, (int)GC_ParseCommand(code, const_cast<char*>(command.c_str())));
}

TEST_F(GCodeParserTest, command_G_rejects_M_params)
{
    std::string command = "G1 X1.75 S200";
    ASSERT_EQ((int)GCODE_ERROR_UNKNOWN_PARAM, (int)GC_ParseCommand(code, const_cast<char*>(command.c_str())));
}

class GCodeParserCommandTest : public ::testing::Test
{
protected:
    std::unique_ptr<Device> device;
    HGCODE code = nullptr;
    GCodeAxisConfig cfg = { 100, 101, 100, 100 };

    std::string secondary_command = "G1 X-1.45 Z+0.3 E-1.11";

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        code = GC_Configure(&cfg);

        std::string command = "G0 F6200 X2.45 Y1.00 Z0.1 E1.764";
        GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    }

    virtual void ValidateArguments(GCodeCommandParams* g)
    {
        ASSERT_EQ(6200, g->fetch_speed);
        ASSERT_EQ(2.45f * cfg.x_steps_per_cm, g->x);
        ASSERT_EQ(1.00f * cfg.y_steps_per_cm, g->y);
        ASSERT_EQ(0.10f * cfg.z_steps_per_cm, g->z);
        ASSERT_EQ(1.76f * cfg.e_steps_per_cm, g->e);
    }

    virtual void ValidateSecondaryArguments(GCodeCommandParams* g)
    {
        ASSERT_EQ(6200, g->fetch_speed);                          //unmodified
        ASSERT_EQ((int16_t)(-1.45f * cfg.x_steps_per_cm), g->x);
        ASSERT_EQ((int16_t)(1.00f * cfg.y_steps_per_cm), g->y);  //unmodified
        ASSERT_EQ((int16_t)(0.30f * cfg.z_steps_per_cm), g->z);
        ASSERT_EQ((int16_t)(-1.11f * cfg.e_steps_per_cm), g->e);
    }

    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(GCodeParserCommandTest, command_G0_type)
{
    ASSERT_EQ((int)GCODE_COMMAND, (int)GC_GetCurrentCommandCode(code) & 0XFF00);
}

TEST_F(GCodeParserCommandTest, command_G0_code)
{
    ASSERT_EQ(0, (int)GC_GetCurrentCommandCode(code) & 0x00FF);
}

TEST_F(GCodeParserCommandTest, command_G0_command)
{
    ASSERT_EQ((int)GCODE_COMMAND | 0x0000, (int)GC_GetCurrentCommandCode(code));
}

TEST_F(GCodeParserCommandTest, command_G0_valid_command)
{
    ASSERT_TRUE(nullptr != GC_GetCurrentCommand(code));
}

TEST_F(GCodeParserCommandTest, command_M_params_for_G0_are_invalid)
{
    ASSERT_TRUE(nullptr == GC_GetCurrentSubCommand(code));
}

TEST_F(GCodeParserCommandTest, comman_G_fetch_speed)
{
    GCodeCommandParams* g = GC_GetCurrentCommand(code);
    ASSERT_EQ(6200, g->fetch_speed);
}

TEST_F(GCodeParserCommandTest, comman_G_x)
{
    GCodeCommandParams* g = GC_GetCurrentCommand(code);
    ASSERT_EQ(2.45f * cfg.x_steps_per_cm, g->x);
}

TEST_F(GCodeParserCommandTest, comman_G_y)
{
    GCodeCommandParams* g = GC_GetCurrentCommand(code);
    ASSERT_EQ(1.00f * cfg.y_steps_per_cm, g->y);
}

TEST_F(GCodeParserCommandTest, comman_G_z)
{
    GCodeCommandParams* g = GC_GetCurrentCommand(code);
    ASSERT_EQ(0.10f * cfg.z_steps_per_cm, g->z);
}

TEST_F(GCodeParserCommandTest, comman_G_e)
{
    GCodeCommandParams* g = GC_GetCurrentCommand(code);
    ASSERT_EQ(1.76f * cfg.e_steps_per_cm, g->e);
}

TEST_F(GCodeParserCommandTest, comment_doesnt_change_params)
{
    std::string command = ";this is the comment in gcode";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));

    command = "G1";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    GCodeCommandParams* g = GC_GetCurrentCommand(code);
    ASSERT_NO_FATAL_FAILURE(ValidateArguments(g));
}

TEST_F(GCodeParserCommandTest, trailing_comment)
{
    std::string command = secondary_command + " ;some additional comment";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    ASSERT_NO_FATAL_FAILURE(ValidateSecondaryArguments(GC_GetCurrentCommand(code)));
}

TEST_F(GCodeParserCommandTest, ignore_spaces_before)
{
    std::string command = std::string("    ") + secondary_command; 
    ASSERT_EQ((int)GCODE_OK_COMMAND_CREATED, (int)GC_ParseCommand(code, const_cast<char*>(command.c_str())));
}

TEST_F(GCodeParserCommandTest, ignore_spaces_before_parameters)
{
    std::string command = std::string("    ") + secondary_command;
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    ASSERT_NO_FATAL_FAILURE(ValidateSecondaryArguments(GC_GetCurrentCommand(code)));
}

TEST_F(GCodeParserCommandTest, ignore_trailing_spaces)
{
    std::string command = secondary_command + std::string("    ");
    ASSERT_EQ((int)GCODE_OK_COMMAND_CREATED, (int)GC_ParseCommand(code, const_cast<char*>(command.c_str())));
}

TEST_F(GCodeParserCommandTest, ignore_trailing_spaces_parameters)
{
    std::string command = secondary_command + std::string("    ");
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    ASSERT_NO_FATAL_FAILURE(ValidateSecondaryArguments(GC_GetCurrentCommand(code)));
}

TEST_F(GCodeParserCommandTest, ignore_spaces_middle)
{
    std::string command = "G1  X-1.45      Z+0.3     E-1.11";
    ASSERT_EQ((int)GCODE_OK_COMMAND_CREATED, (int)GC_ParseCommand(code, const_cast<char*>(command.c_str())));
}

TEST_F(GCodeParserCommandTest, next_command)
{
    std::string command = secondary_command;
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    ASSERT_NO_FATAL_FAILURE(ValidateSecondaryArguments(GC_GetCurrentCommand(code)));
}

TEST_F(GCodeParserCommandTest, invalid_command_doesnot_invalidate_data)
{
    std::string command = "Q92";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    
    command = "G1";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));

    GCodeCommandParams* g = GC_GetCurrentCommand(code);
    ASSERT_NO_FATAL_FAILURE(ValidateArguments(g));
}

TEST_F(GCodeParserCommandTest, invalid_parameter_discards_parameter_changes)
{
    std::string command = "G1 X1.21 Z4.32 Q33";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));

    command = "G1";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    GCodeCommandParams* g = GC_GetCurrentCommand(code);
    ASSERT_NO_FATAL_FAILURE(ValidateArguments(g));
}

class GCodeParserSubCommandTest : public ::testing::Test
{
protected:
    std::unique_ptr<Device> device;
    HGCODE code = nullptr;
    GCodeAxisConfig cfg = { 100, 101, 100, 100 };

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        code = GC_Configure(&cfg);

        std::string command = "M140 I22 R4 S110 P1";
        GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    }

    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(GCodeParserSubCommandTest, command_M140_type)
{
    ASSERT_EQ((int)GCODE_SUBCOMMAND, (int)GC_GetCurrentCommandCode(code) & 0XFF00);
}

TEST_F(GCodeParserSubCommandTest, command_M140_code)
{
    ASSERT_EQ(140, (int)GC_GetCurrentCommandCode(code) & 0x00FF);
}

TEST_F(GCodeParserSubCommandTest, command_M140_command)
{
    ASSERT_EQ((int)GCODE_SUBCOMMAND | 140, (int)GC_GetCurrentCommandCode(code));
}

TEST_F(GCodeParserSubCommandTest, command_G140_valid_command)
{
    ASSERT_TRUE(nullptr != GC_GetCurrentSubCommand(code));
}

TEST_F(GCodeParserSubCommandTest, command_G_params_for_M140_are_invalid)
{
    ASSERT_TRUE(nullptr == GC_GetCurrentCommand(code));
}

TEST_F(GCodeParserSubCommandTest, comman_M_s)
{
    GCodeSubCommandParams* m = GC_GetCurrentSubCommand(code);
    ASSERT_EQ(110, m->s);
}

TEST_F(GCodeParserSubCommandTest, comman_M_p)
{
    GCodeSubCommandParams* m = GC_GetCurrentSubCommand(code);
    ASSERT_EQ(1, m->p);
}

TEST_F(GCodeParserSubCommandTest, comman_M_i)
{
    GCodeSubCommandParams* m = GC_GetCurrentSubCommand(code);
    ASSERT_EQ(22, m->i);
}

TEST_F(GCodeParserSubCommandTest, comman_M_r)
{
    GCodeSubCommandParams* m = GC_GetCurrentSubCommand(code);
    ASSERT_EQ(4, m->r);
}

TEST_F(GCodeParserSubCommandTest, comman_G_doesnt_override_M_state)
{
    std::string command = "G1 X22 Y4 Z110 E1";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    ASSERT_TRUE(nullptr != GC_GetCurrentCommand(code));

    command = "M109";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    GCodeSubCommandParams* m = GC_GetCurrentSubCommand(code);
    ASSERT_EQ(110, m->s);
    ASSERT_EQ(1, m->p);
    ASSERT_EQ(22, m->i);
    ASSERT_EQ(4, m->r);
}

class GCodeParserDialectTest : public ::testing::Test
{
protected:
    std::unique_ptr<Device> device;
    HGCODE code = nullptr;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        GCodeAxisConfig cfg = { 1, 1, 1, 100 };
        code = GC_Configure(&cfg);
    }

    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(GCodeParserDialectTest, wanhao)
{
    FILE* f = nullptr;
    fopen_s(&f, "wanhao.gcode", "r");
    ASSERT_TRUE(nullptr != f) << "required file not found";

    size_t commands = 0;
    size_t comments = 0;
    size_t line_length = 0;

    char symbol = ' ';
    while (!feof(f))
    {
        std::string line = "";
        while (symbol)
        {
            fread_s(&symbol, 1, 1, 1, f);
            if (symbol == '\n' || symbol == '\r')
            {
                break;
            }
            line.append(&symbol, 1);
        }
        if (line.size())
        {
            GCODE_ERROR result = GC_ParseCommand(code, const_cast<char*>(line.c_str()));
            line_length = std::max(line_length, line.size());
            switch (result)
            {
            case GCODE_OK_COMMAND_CREATED:
                ++commands;
                break;
            case GCODE_OK_NO_COMMAND:
                ++comments;
                break;
            default:
                FAIL() << "invalid line " << line.c_str();
                break;
            }
        }
    }
    std::cout << "Wanhao dialect file parsed successfully. " << std::endl 
        << "    Total lines: " << comments + commands << std::endl
        << "    Commands: " << commands << std::endl 
        << "    Comments and empty lines: " << comments << std::endl
        << "    Max line length: " << line_length << std::endl;

    fclose(f);
}