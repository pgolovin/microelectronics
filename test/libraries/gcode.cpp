
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

TEST_F(GCodeParserTest, trailing_comment_nospace)
{
    std::string command = "M104;some additional comment";
    ASSERT_EQ((int)GCODE_ERROR_UNKNOWN_PARAM, (int)GC_ParseCommand(code, const_cast<char*>(command.c_str())));
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

TEST_F(GCodeParserTest, compress_nothing)
{
    ASSERT_EQ(0U, GC_CompressCommand(code, nullptr));
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
        ASSERT_EQ(2.45f * cfg.x_steps_per_mm, g->x);
        ASSERT_EQ(1.00f * cfg.y_steps_per_mm, g->y);
        ASSERT_EQ(0.10f * cfg.z_steps_per_mm, g->z);
        ASSERT_EQ(1.76f * cfg.e_steps_per_mm, g->e);
    }

    virtual void ValidateSecondaryArguments(GCodeCommandParams* g)
    {
        ASSERT_EQ(6200, g->fetch_speed);                          //unmodified
        ASSERT_EQ((int16_t)(-1.45f * cfg.x_steps_per_mm), g->x);
        ASSERT_EQ((int16_t)(1.00f * cfg.y_steps_per_mm), g->y);  //unmodified
        ASSERT_EQ((int16_t)(0.30f * cfg.z_steps_per_mm), g->z);
        ASSERT_EQ((int16_t)(-1.11f * cfg.e_steps_per_mm), g->e);
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
    ASSERT_EQ(2.45f * cfg.x_steps_per_mm, g->x);
}

TEST_F(GCodeParserCommandTest, comman_G_y)
{
    GCodeCommandParams* g = GC_GetCurrentCommand(code);
    ASSERT_EQ(1.00f * cfg.y_steps_per_mm, g->y);
}

TEST_F(GCodeParserCommandTest, comman_G_z)
{
    GCodeCommandParams* g = GC_GetCurrentCommand(code);
    ASSERT_EQ(0.10f * cfg.z_steps_per_mm, g->z);
}

TEST_F(GCodeParserCommandTest, comman_G_e)
{
    GCodeCommandParams* g = GC_GetCurrentCommand(code);
    ASSERT_EQ(1.76f * cfg.e_steps_per_mm, g->e);
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

class GCodeCompilerTest : public ::testing::Test
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

        std::string command = "G1 F6200 X2.45 Y1.00 Z0.1 E1.764";
        GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    }

    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }
};

TEST_F(GCodeCompilerTest, compress_command)
{
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    ASSERT_EQ(GCODE_CHUNK_SIZE, GC_CompressCommand(code, data.data()));
}

TEST_F(GCodeCompilerTest, compress_command_data)
{
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());
    GCodeCommandParams* params = reinterpret_cast<GCodeCommandParams*>(data.data() + sizeof(parameterType));
    GCodeCommandParams* g = GC_GetCurrentCommand(code);
    ASSERT_EQ(g->fetch_speed, params->fetch_speed);
    ASSERT_EQ(g->x, params->x);
    ASSERT_EQ(g->y, params->y);
    ASSERT_EQ(g->z, params->z);
    ASSERT_EQ(g->e, params->e);
}

struct CompilerCommand
{
    std::string         test_name;
    std::string         command_line;
    GCODE_COMMAND_TYPE  type;
    uint16_t            code;
};

class GCodeCommandCompilerTest : public ::testing::TestWithParam<CompilerCommand>
{
public:

protected:
    std::unique_ptr<Device> device;
    HGCODE code = nullptr;
    GCodeAxisConfig cfg = { 100, 101, 100, 100 };

    std::vector<uint8_t> data;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        code = GC_Configure(&cfg);

        data.resize(GCODE_CHUNK_SIZE);
    }

    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }
};

TEST_P(GCodeCommandCompilerTest, command_code)
{
    const auto& params = GetParam();
    std::string command = params.command_line;
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    GC_CompressCommand(code, data.data());
    uint16_t command_code = *reinterpret_cast<uint16_t*>(data.data());
    ASSERT_TRUE(0 != (params.type & command_code));
    ASSERT_TRUE(params.code == (command_code & 0x00ff));
}

INSTANTIATE_TEST_SUITE_P(
    CommandCodeTest, GCodeCommandCompilerTest,
    ::testing::Values(
        CompilerCommand{ "idling_move", "G0 F6200 X2.45 Y1.00 Z0.1 E1.764", GCODE_COMMAND, GCODE_MOVE },
        CompilerCommand{ "working_move", "G1 F6200 X2.45 Y1.00 Z0.1 E1.764", GCODE_COMMAND, GCODE_MOVE },
        CompilerCommand{ "homing_move", "G28 X2.45 Y1.00", GCODE_COMMAND, GCODE_HOME },
        CompilerCommand{ "set_value", "G92 X0 Y0", GCODE_COMMAND, GCODE_SET },
        CompilerCommand{ "heat_nozzle", "M104 S256", GCODE_SUBCOMMAND, GCODE_SET_NOZZLE_TEMPERATURE },
        CompilerCommand{ "heat_table", "M140 S256", GCODE_SUBCOMMAND, GCODE_SET_TABLE_TEMPERATURE },
        CompilerCommand{ "enable_cooler", "M106 S256", GCODE_SUBCOMMAND, GCODE_SET_COOLER_SPEED },
        CompilerCommand{ "disable_cooler", "M107", GCODE_SUBCOMMAND, GCODE_SET_COOLER_SPEED },
        CompilerCommand{ "wait_nozzle_to_heat", "M109 S256", GCODE_SUBCOMMAND, GCODE_WAIT_NOZZLE },
        CompilerCommand{ "wait_table_to_heat", "M190 S256", GCODE_SUBCOMMAND, GCODE_WAIT_TABLE }

    ),
    [](const ::testing::TestParamInfo<GCodeCommandCompilerTest::ParamType>& info)
    {
        std::ostringstream out;
        return info.param.test_name;
    }
);

class GCodeDecompilerTest : public ::testing::Test
{
protected:

    std::unique_ptr<Device> device;
    HGCODE code = nullptr;
    GCodeAxisConfig cfg = { 100, 100, 100, 100 };
    std::vector<uint8_t> compiled_data;
    GCodeFunctionList functions;

    static uint32_t void_parameter;
    static const uint32_t check_parameter = 42;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);
        code = GC_Configure(&cfg);
    }
};

TEST_F(GCodeDecompilerTest, decompile_cannot_without_data)
{
    GCODE_COMMAND_LIST command_id;
    ASSERT_TRUE(nullptr == GC_DecompileFromBuffer(nullptr, &command_id));
}

TEST_F(GCodeDecompilerTest, decompile_cannot_without_command_id)
{
    std::string command = "G0 F1600 X1.0 Y2.0 Z0.1 E0.12";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());

    ASSERT_TRUE(nullptr == GC_DecompileFromBuffer(data.data(), nullptr));
}

TEST_F(GCodeDecompilerTest, decompile_can_do_command)
{
    std::string command = "G0 F1600 X1.0 Y2.0 Z0.1 E0.12";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());

    GCODE_COMMAND_LIST command_id;

    ASSERT_TRUE(nullptr != GC_DecompileFromBuffer(data.data(), &command_id));
}

TEST_F(GCodeDecompilerTest, decompile_command)
{
    std::string command = "G0 F1600 X1.0 Y2.45 Z0.1 E0.12";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());

    GCODE_COMMAND_LIST command_id;
    GCodeCommandParams* params = GC_DecompileFromBuffer(data.data(), &command_id);

    ASSERT_EQ(GCODE_MOVE, command_id);
    ASSERT_EQ(1600, params->fetch_speed);
    ASSERT_EQ((int16_t)(1.0 * cfg.x_steps_per_mm), params->x);
    ASSERT_EQ((int16_t)(2.45 * cfg.y_steps_per_mm), params->y);
    ASSERT_EQ((int16_t)(0.1 * cfg.z_steps_per_mm), params->z);
    ASSERT_EQ((int16_t)(0.12 * cfg.e_steps_per_mm), params->e);
}

TEST_F(GCodeDecompilerTest, decompile_subcommand)
{
    std::string command = "M107 S255";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());

    GCODE_COMMAND_LIST command_id;
    ASSERT_TRUE(nullptr == GC_DecompileFromBuffer(data.data(), &command_id));
}

class GCodeExecutorTest : public ::testing::Test
{
protected:

    struct Execution_Test
    {
        uint16_t command_id;
        GCodeCommandParams command_execution_parameters;
        GCodeSubCommandParams subcommand_execution_parameters;
    };

    std::unique_ptr<Device> device;
    HGCODE code = nullptr;
    GCodeAxisConfig cfg = { 100, 101, 100, 100 };
    std::vector<uint8_t> compiled_data;
    GCodeFunctionList functions;
    
    static std::vector<Execution_Test> results_list;
    static uint32_t void_parameter;
    static const uint32_t check_parameter = 42;

    virtual void SetUp()
    {
        DeviceSettings ds;
        device = std::make_unique<Device>(ds);
        AttachDevice(*device);

        code = GC_Configure(&cfg);

        results_list.clear();
        functions.commands[GCODE_MOVE] = GCodeExecutorTest::move;
        functions.commands[GCODE_HOME] = GCodeExecutorTest::home;
        functions.commands[GCODE_SET]  = GCodeExecutorTest::set;

        functions.subcommands[GCODE_SET_NOZZLE_TEMPERATURE] = setNozzleTemp;
        functions.subcommands[GCODE_SET_COOLER_SPEED] = disableCooler;
        functions.subcommands[GCODE_WAIT_NOZZLE] = waitNozzle;
    }

    virtual void TearDown()
    {
        DetachDevice();
        device = nullptr;
    }

    static GCODE_COMMAND_STATE move(GCodeCommandParams* parameters, void* param)
    {
        void_parameter = *(uint32_t*)param;
        results_list.emplace_back(Execution_Test{ (uint16_t)GCODE_MOVE | GCODE_COMMAND, *parameters, GCodeSubCommandParams{ 0 } });
        return GCODE_OK;
    }
    static GCODE_COMMAND_STATE home(GCodeCommandParams* parameters, void* param)
    {
        void_parameter = *(uint32_t*)param;
        results_list.emplace_back(Execution_Test{ (uint16_t)GCODE_HOME | GCODE_COMMAND, *parameters, GCodeSubCommandParams{ 0 } });
        return GCODE_OK;
    }
    static GCODE_COMMAND_STATE set(GCodeCommandParams* parameters, void* param)
    {
        void_parameter = *(uint32_t*)param;
        results_list.emplace_back(Execution_Test{ (uint16_t)GCODE_SET | GCODE_COMMAND, *parameters, GCodeSubCommandParams{ 0 } });
        return GCODE_OK;
    }
    static GCODE_COMMAND_STATE setNozzleTemp(GCodeSubCommandParams* parameters, void* param)
    {
        void_parameter = *(uint32_t*)param;
        results_list.emplace_back(Execution_Test{ (uint16_t)GCODE_SET_NOZZLE_TEMPERATURE | GCODE_SUBCOMMAND, GCodeCommandParams{ 0 }, *parameters });
        return GCODE_OK;
    }
    static GCODE_COMMAND_STATE disableCooler(GCodeSubCommandParams* parameters, void* param)
    {
        void_parameter = *(uint32_t*)param;
        results_list.emplace_back(Execution_Test{ (uint16_t)GCODE_SET_COOLER_SPEED | GCODE_SUBCOMMAND, GCodeCommandParams{ 0 }, *parameters });
        return GCODE_OK;
    }

    static GCODE_COMMAND_STATE waitNozzle(GCodeSubCommandParams*, void* param)
    {
        void_parameter = *(uint32_t*)param;
        return GCODE_INCOMPLETE;
    }
};

std::vector<GCodeExecutorTest::Execution_Test> GCodeExecutorTest::results_list = {};
uint32_t GCodeExecutorTest::void_parameter = 0;

TEST_F(GCodeExecutorTest, execute_empty_command_list)
{
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    ASSERT_EQ((int)GCODE_FATAL_ERROR_NO_COMMAND, GC_ExecuteFromBuffer(nullptr, (void*)&check_parameter, data.data()));
}

TEST_F(GCodeExecutorTest, execute_empty_buffer)
{
    ASSERT_EQ((int)GCODE_FATAL_ERROR_NO_DATA, GC_ExecuteFromBuffer(&functions, (void*)&check_parameter, 0));
}

TEST_F(GCodeExecutorTest, execute_invalid_buffer)
{
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE, 0);
    ASSERT_EQ((int)GCODE_FATAL_ERROR_UNKNOWN_COMMAND, GC_ExecuteFromBuffer(&functions, (void*)&check_parameter, data.data()));
}

TEST_F(GCodeExecutorTest, execute_command)
{
    std::string command = "G0 F1600 X1.0 Y2.0 Z0.1 E0.12";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());
    ASSERT_EQ((int)GCODE_OK, GC_ExecuteFromBuffer(&functions, (void*)&check_parameter, data.data()));
}

TEST_F(GCodeExecutorTest, execute_move_command)
{
    std::string command = "G0 F1600 X1.0 Y2.0 Z0.1 E0.12";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());
    GC_ExecuteFromBuffer(&functions, (void*)&check_parameter, data.data());
    ASSERT_EQ(1, results_list.size());
    ASSERT_EQ(GCODE_MOVE, results_list[0].command_id & 0x00ff);
}

TEST_F(GCodeExecutorTest, execute_home_command)
{
    std::string command = "G28 X0 Y0";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());
    GC_ExecuteFromBuffer(&functions, (void*)&check_parameter, data.data());
    ASSERT_EQ(1, results_list.size());
    ASSERT_EQ(GCODE_HOME, results_list[0].command_id & 0x00ff);
}

TEST_F(GCodeExecutorTest, execute_set_command)
{
    std::string command = "G92 X0 Y0";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());
    GC_ExecuteFromBuffer(&functions, (void*)&check_parameter, data.data());
    ASSERT_EQ(1, results_list.size());
    ASSERT_EQ(GCODE_SET, results_list[0].command_id & 0x00ff);
}

TEST_F(GCodeExecutorTest, execute_check_additional_parameters)
{
    std::string command = "G92 X0 Y0";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());
    GC_ExecuteFromBuffer(&functions, (void*)&check_parameter, data.data());
    ASSERT_EQ(check_parameter, void_parameter);
}

TEST_F(GCodeExecutorTest, execution_parameters)
{
    std::string command = "G1 F1600 X1.0 Y2.45 Z0.1 E0.12";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());
    GC_ExecuteFromBuffer(&functions, (void*)&check_parameter, data.data());
    ASSERT_EQ(1, results_list.size());
    ASSERT_EQ(1600, results_list[0].command_execution_parameters.fetch_speed);
    ASSERT_EQ((int16_t)(1.0 * cfg.x_steps_per_mm), results_list[0].command_execution_parameters.x);
    ASSERT_EQ((int16_t)(2.45 * cfg.y_steps_per_mm), results_list[0].command_execution_parameters.y);
    ASSERT_EQ((int16_t)(0.1 * cfg.z_steps_per_mm), results_list[0].command_execution_parameters.z);
    ASSERT_EQ((int16_t)(0.12 * cfg.e_steps_per_mm), results_list[0].command_execution_parameters.e);
}

TEST_F(GCodeExecutorTest, execute_subcommand)
{
    std::string command = "M104 S256";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());
    ASSERT_EQ((int)GCODE_OK, GC_ExecuteFromBuffer(&functions, (void*)&check_parameter, data.data()));
}

TEST_F(GCodeExecutorTest, execute_set_temperature_subcommand)
{
    std::string command = "M104 S256";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());
    GC_ExecuteFromBuffer(&functions, (void*)&check_parameter, data.data());
    ASSERT_EQ(1, results_list.size());
    ASSERT_TRUE(0 != (GCODE_SUBCOMMAND & results_list[0].command_id));
    ASSERT_EQ(GCODE_SET_NOZZLE_TEMPERATURE, results_list[0].command_id & 0x00ff);
}

TEST_F(GCodeExecutorTest, execute_subcommand_parameters)
{
    // M107 command is the same as M106, but S parameter is forced to 0.
    std::string command = "M107 I2 S254";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());
    GC_ExecuteFromBuffer(&functions, (void*)&check_parameter, data.data());
    ASSERT_EQ(1, results_list.size());
    ASSERT_EQ(2, results_list[0].subcommand_execution_parameters.i);
    ASSERT_EQ(0, results_list[0].subcommand_execution_parameters.s);
}

TEST_F(GCodeExecutorTest, execute_forward_return_value)
{
    std::string command = "M109 I2 S254";
    GC_ParseCommand(code, const_cast<char*>(command.c_str()));
    std::vector<uint8_t> data(GCODE_CHUNK_SIZE);
    GC_CompressCommand(code, data.data());
    GC_ExecuteFromBuffer(&functions, (void*)&check_parameter, data.data());
    ASSERT_EQ((int)GCODE_INCOMPLETE, GC_ExecuteFromBuffer(&functions, (void*)&check_parameter, data.data()));
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

TEST_F(GCodeParserDialectTest, wanhao_compiled)
{
    FILE* f = nullptr;
    fopen_s(&f, "wanhao.gcode", "r");
    ASSERT_TRUE(nullptr != f) << "required file not found";

    std::vector<uint8_t> data(GCODE_CHUNK_SIZE, 0);
    std::vector<uint8_t> compiled_file;

    size_t commands = 0;
    size_t comments = 0;
    size_t data_excess = 0;
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
            uint32_t excess = 0;
            GCODE_ERROR result = GC_ParseCommand(code, const_cast<char*>(line.c_str()));
            line_length = std::max(line_length, line.size());
            if (GC_GetCurrentCommandCode(code) & GCODE_COMMAND)
            {
                excess = GCODE_CHUNK_SIZE - sizeof(GCodeCommandParams) - sizeof(uint16_t);
            }
            else if (GC_GetCurrentCommandCode(code) & GCODE_COMMAND)
            {
                excess = GCODE_CHUNK_SIZE - sizeof(GCodeSubCommandParams) - sizeof(uint16_t);
            }

            if (0 != GC_CompressCommand(code, data.data()))
            {
                compiled_file.insert(compiled_file.end(), data.begin(), data.end());
                data_excess += excess;
            }
            switch (result)
            {
            case GCODE_OK_COMMAND_CREATED:
                ++commands;
                break;
            case GCODE_OK_NO_COMMAND:
                ++comments;
                break;
            default:
                break;
            }
        }
    }
    ASSERT_GE(commands, compiled_file.size() / GCODE_CHUNK_SIZE);
    ASSERT_EQ(0, compiled_file.size() % GCODE_CHUNK_SIZE);

    std::cout << "Wanhao dialect file compiled successfully. " << std::endl
        << "    Total lines: " << comments + commands << std::endl
        << "    Commands: " << commands << std::endl
        << "    Compiled commands: " << compiled_file.size() / GCODE_CHUNK_SIZE << std::endl
        << "    Size of compiled file: " << compiled_file.size() << std::endl
        << "    Data Excess: " << data_excess << " / "<< (size_t)round(100.0 * data_excess/ compiled_file.size() ) << "% of total" << std::endl
        << "    Allocated data chunks: " << compiled_file.size() / 512 + 1 << std::endl;

    fclose(f);
}