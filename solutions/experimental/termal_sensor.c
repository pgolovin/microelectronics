#include "main.h"
#include "termal_sensor.h"

#include "include/spibus.h"
#include "include/sdcard.h"
#include "include/display.h"
#include "include/touch.h"
#include "include/user_interface.h"
#include "include/gcode.h"
#include "ff.h"
#include "diskio.h"

#include "include/equalizer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TermalAngle 0.612f
#define TermalOffset -1446.923f

#define HeatingAngle 0.35714f
#define HeatingOffset -760.71428f

typedef struct
{
    uint32_t steps;
    uint32_t speed;
    uint32_t current_speed;
    bool     running;
    bool     status;
    GPIO_PinState direction;
    
    GPIO_TypeDef *step_port;
    uint16_t      step_pin;
    GPIO_TypeDef *dir_port;
    uint16_t      dir_pin;
} EngineState;

typedef struct
{
    HEQUALIZER  equlizer;
    uint16_t    value;
    uint16_t    voltage[15];
    uint16_t    step;

    float       temperature;
} TermalSensor;

typedef struct
{
    bool initialized;
    HSPIBUS main_bus;
    HSPIBUS secondary_bus;
    HSPIBUS ram_bus;
    HDISPLAY display;
    HTOUCH touch;
    
    HSDCARD card;
    FATFS fs;
    bool card_state;
    
    HSDCARD ram;
    bool ram_state;

    HGCODE      gcode;
    UI ui;
    HIndicator  sdcard_indicator;
    HIndicator  ram_indicator;
    HIndicator  nozzle_indicator;
    HIndicator  table_indicator;
    HIndicator  timer;
    
    uint16_t    timer_value;
    bool        timer_updated;
    
    TermalSensor nozzle;
    TermalSensor table;
    
    float current_temperature;

// engines
    EngineState x;
    EngineState y;
    EngineState z;
// E - Calibration: 10000 steps = 104mm 1step = 0.0104mm
    EngineState e;

    Rect text_place;
    Rect status_bar;
} Experimental;

enum EXECUTION_STATUS
{
    ES_OK,
    ES_WARNING,
    ES_FAIL,
};

#define STEPS 1600
#define CALIBRATION_STEPS 10000

static uint16_t status_colors[] = {0x07E0, 0xFFe0, 0xF800};
static char status_messages[3][7] = {"[ OK ]", "[WARN]", "[FAIL]"};

void PrintStatusMessage(HDISPLAY display, uint8_t status, char* msg)
{
    DISPLAY_SetFontColor(display, status_colors[status]);
    DISPLAY_SetBackgroundColor(display, 0x0007);
    DISPLAY_Print(display, status_messages[status]);
    DISPLAY_SetFontColor(display, 0x5BDF);
    DISPLAY_Print(display, ": ");
    DISPLAY_Print(display, msg);
    DISPLAY_Print(display, "\n");
}

void WritePrinterStatus(Experimental* exp, const char* status_message)
{
    if (SDCARD_OK != SDCARD_IsInitialized(exp->card))
    {
        return;
    }
    
	FRESULT result;
	result = f_mount(&exp->fs, "", 0);
    if (result != RES_OK)
	{
        PrintStatusMessage(exp->display, ES_FAIL, "Card mount");
    }

	FIL log_file;
	result = f_open(&log_file, "prnt_log.txt", FA_CREATE_NEW | FA_WRITE);
    
    if (result == FR_EXIST)
    {
        result = f_open(&log_file, "prnt_log.txt", FA_OPEN_APPEND | FA_WRITE);
    }
    
 	if (result == FR_OK)
	{
        f_printf(&log_file, "%s;\n", status_message);
		f_close(&log_file);
	}
    else
    {
        char name[32];
        sprintf(name, "Err 0x%02x", result);
        PrintStatusMessage(exp->display, ES_FAIL, name);
    }
    
	f_mount(0, "", 0);
}

float calculateTemperature(uint16_t adc_value)
{
    if (adc_value < 2600)
    {
        return adc_value*HeatingAngle + HeatingOffset;
    }
    
    return adc_value*TermalAngle + TermalOffset;
}

uint16_t calculateIncrement(float temperature)
{
    if (temperature > 200)
    {
        return (temperature - TermalOffset)/TermalAngle;
    }
    
    return (temperature - HeatingOffset)/HeatingAngle;
}

void updateValue(Experimental* exp, TermalSensor* sensor, float termal_increment)
{
    sensor->value += termal_increment;
    EQ_SetTargetValue(sensor->equlizer, sensor->value);
    char value[] = "NOZZLE=0000, TABLE=0000";
    sprintf(value, "N=%d; T=%d", exp->nozzle.value, exp->table.value);
    PrintStatusMessage(exp->display, ES_WARNING, value);
    
    exp->timer_value = 0;
    UI_SetIndicatorValue(exp->ui, exp->timer, false);
}


static bool savePrinterState(void* metadata)
{
    Experimental* exp = (Experimental*)metadata;
    if (!exp->ram_state)
    {
        PrintStatusMessage(exp->display, ES_FAIL, "RAM state");
        return false;
    }
    
    uint8_t buffer[512];
    *((uint32_t*)(&buffer[0])) = 'ram0';
    *((uint32_t*)(&buffer[4])) = 'gard';
    *((uint16_t*)(&buffer[8])) = EQ_GetTargetValue(exp->nozzle.equlizer);
    *((uint16_t*)(&buffer[10])) = EQ_GetTargetValue(exp->table.equlizer);
    if (SDCARD_OK == SDCARD_WriteSingleBlock(exp->ram, buffer, 0))
    {
        PrintStatusMessage(exp->display, ES_OK, "State saved");
    }
    else 
    {
        PrintStatusMessage(exp->display, ES_FAIL, "State save");
    }
    return true;
}

static bool restorePrinterState(void* metadata)
{
    Experimental* exp = (Experimental*)metadata;
    if (!exp->ram_state)
    {
        PrintStatusMessage(exp->display, ES_FAIL, "RAM state");
        return false;
    }
    uint8_t buffer[512];
    if (SDCARD_OK == SDCARD_ReadSingleBlock(exp->ram, buffer, 0))
    {
        PrintStatusMessage(exp->display, ES_OK, "State restored");
    }
    else 
    {
        PrintStatusMessage(exp->display, ES_FAIL, "State restore");
        return false;
    }
    uint32_t* ram_gard = (uint32_t*)buffer;
    if (ram_gard[0] != 'ram0' && ram_gard[1] != 'gard')
    {
        PrintStatusMessage(exp->display, ES_FAIL, "Invalid format");
        return false;
    }
    uint16_t t = *((uint16_t*)&buffer[8]);
    if (t < 3000 && t > 2000)
    {
        exp->nozzle.value   = t;
        updateValue(exp, &exp->nozzle, 0);
    }
    else 
    {
        PrintStatusMessage(exp->display, ES_FAIL, "Invalid nozzle T");
    }
    t = *((uint16_t*)&buffer[10]);
    if (t < 4000 && t > 500)
    {
        exp->table.value    = t;
        updateValue(exp, &exp->table, 0);
    }
    else 
    {
        PrintStatusMessage(exp->display, ES_FAIL, "Invalid table T");
    }
    return true;
}

bool add100(void* metadata)
{
    updateValue((Experimental*)metadata, &((Experimental*)metadata)->nozzle, 100);
    return true;
}
bool add50(void* metadata)
{
    updateValue((Experimental*)metadata, &((Experimental*)metadata)->nozzle,  50);
    return true;
}
bool add10(void* metadata)
{
    updateValue((Experimental*)metadata, &((Experimental*)metadata)->nozzle,  10);
    return true;
}
bool remove100(void* metadata)
{
    updateValue((Experimental*)metadata, &((Experimental*)metadata)->nozzle, -10);
    return true;
}

bool tableRemove100(void* metadata)
{
    updateValue((Experimental*)metadata, &((Experimental*)metadata)->table, -100);
    return true;
}
bool tableAdd100(void* metadata)
{
    updateValue((Experimental*)metadata, &((Experimental*)metadata)->table,  100);
    return true;
}

bool reset(void* metadata)
{
    Experimental* exp   = (Experimental*)metadata;
    exp->nozzle.value   = 2200;
    exp->table.value    = 3500;
    
    updateValue(exp, &exp->nozzle, 0);
    updateValue(exp, &exp->table, 0);
    
    exp->timer_value = 0;
    UI_SetIndicatorValue(exp->ui, exp->timer, false);
    
    return true;
}

bool e_engine(void* metadata)
{
    Experimental* exp = (Experimental*)metadata;
    
    if (exp->current_temperature > 230)
    { 
        exp->e.running = !exp->e.running;
        PrintStatusMessage(exp->display, ES_OK, exp->e.running ? "Extruder running" : "Extruder stopped");
    }
    else 
    {
        exp->e.running = false;
        PrintStatusMessage(exp->display, ES_WARNING, "Low Temperature");
    }
    return true;
}

bool toggle_nozzle_cooler(void* metadata)
{
    HAL_GPIO_TogglePin(EXTRUDER_COOLER_CONTROL_GPIO_Port, EXTRUDER_COOLER_CONTROL_Pin);
    return true;
}

bool run_engine(void* metadata)
{
    EngineState* engine = (EngineState*)metadata;
    engine->steps = STEPS;
    engine->running = !(engine->running);
    return true;
}

bool calibrate_engine(void* metadata)
{
    EngineState* engine = (EngineState*)metadata;
    engine->steps = CALIBRATION_STEPS;
    engine->running = true;
    return true;
}

bool dir_engine(void* metadata)
{
    EngineState* engine = (EngineState*)metadata;
    if (GPIO_PIN_SET == engine->direction)
    {
        engine->direction = GPIO_PIN_RESET;
    }
    else
    {
        engine->direction = GPIO_PIN_SET;
    }
    
    HAL_GPIO_WritePin(engine->dir_port, engine->dir_pin, engine->direction);
    return true;
}

void resetEngine(Experimental* exp, EngineState* eng, char prefix)
{
    eng->status = false;
    dir_engine(eng);
    eng->steps = STEPS * 2;
    eng->speed = rand() % 3 + 1;
    eng->current_speed = eng->speed;
    eng->running = true;
}

void onHeat(void* param)
{
    HAL_GPIO_WritePin(EXTRUDER_HEATER_CONTROL_GPIO_Port, EXTRUDER_HEATER_CONTROL_Pin, GPIO_PIN_SET);
}

void onCool(void* param)
{
    HAL_GPIO_WritePin(EXTRUDER_HEATER_CONTROL_GPIO_Port, EXTRUDER_HEATER_CONTROL_Pin, GPIO_PIN_RESET);
}

void onTableHeat(void* param)
{
    HAL_GPIO_WritePin(TABLE_HEATER_CONTROL_GPIO_Port, TABLE_HEATER_CONTROL_Pin, GPIO_PIN_RESET);
}

void onTableCool(void* param)
{
    HAL_GPIO_WritePin(TABLE_HEATER_CONTROL_GPIO_Port, TABLE_HEATER_CONTROL_Pin, GPIO_PIN_SET);
}

void onTemperatureReached(void* param)
{
    Experimental* exp = (Experimental*)param;
    exp->timer_value = 300; // 300 seconds
    exp->timer_updated = true;
}

HEXPERIMENTAL EXPERIMENTAL_Configure(SPI_HandleTypeDef* hspi, SPI_HandleTypeDef* hspi_secondary, SPI_HandleTypeDef* hspi_ram)
{
	Experimental* exp = malloc(sizeof(Experimental));
    exp->initialized = false;
	
    exp->main_bus       = SPIBUS_Configure(hspi, HAL_MAX_DELAY);
    exp->secondary_bus  = SPIBUS_Configure(hspi_secondary, HAL_MAX_DELAY);
    exp->ram_bus        = SPIBUS_Configure(hspi_ram, HAL_MAX_DELAY);

    exp->nozzle.value = 2200;
    exp->nozzle.voltage[0] = 100;
    exp->nozzle.step = 0;
    
    exp->table.value = 3400;    
    exp->table.voltage[0] = 100;
    exp->table.step = 0;
    
    exp->e.running   = false;
    exp->e.direction = GPIO_PIN_RESET;
    exp->e.step_port = E_ENG_STEP_GPIO_Port;
    exp->e.step_pin  = E_ENG_STEP_Pin;
    exp->e.dir_port  = E_ENG_DIR_GPIO_Port;
    exp->e.dir_pin   = E_ENG_DIR_Pin;
    exp->e.speed     = 4;
    exp->e.status    = false;
    
    exp->y.running   = false;
    exp->y.steps     = 0;
    exp->y.direction = GPIO_PIN_RESET;
    exp->y.step_port = Y_ENG_STEP_GPIO_Port;
    exp->y.step_pin  = Y_ENG_STEP_Pin;
    exp->y.dir_port  = Y_ENG_DIR_GPIO_Port;
    exp->y.dir_pin   = Y_ENG_DIR_Pin;
    exp->y.speed     = 1;
    exp->y.current_speed = 1;
    exp->y.status    = false;
    
    exp->x.running   = false;
    exp->x.steps     = 0;
    exp->x.direction = GPIO_PIN_RESET;
    exp->x.step_port = X_ENG_STEP_GPIO_Port;
    exp->x.step_pin  = X_ENG_STEP_Pin;
    exp->x.dir_port  = X_ENG_DIR_GPIO_Port;
    exp->x.dir_pin   = X_ENG_DIR_Pin;
    exp->x.speed     = 1;
    exp->x.current_speed = 1;
    exp->x.status    = false;
    
    exp->z.running   = false;
    exp->z.steps     = 0;
    exp->z.direction = GPIO_PIN_RESET;
    exp->z.step_port = Z_ENG_STEP_GPIO_Port;
    exp->z.step_pin  = Z_ENG_STEP_Pin;
    exp->z.dir_port  = Z_ENG_DIR_GPIO_Port;
    exp->z.dir_pin   = Z_ENG_DIR_Pin;
    exp->z.speed     = 1;
    exp->z.status    = false;
    
    exp->card           = SDCARD_Configure(exp->main_bus, SDCARD_CS_GPIO_Port, SDCARD_CS_Pin);
    exp->card_state     = (SDCARD_OK == SDCARD_Init(exp->card));
    SDCARD_ReadBlocksNumber(exp->card);
    
    exp->ram           = SDCARD_Configure(exp->ram_bus, RAM_CS_GPIO_Port, RAM_CS_Pin);
    exp->ram_state     = (SDCARD_OK == SDCARD_Init(exp->ram));
    SDCARD_ReadBlocksNumber(exp->ram);
    
    DisplayConfig config = 
    {
        exp->main_bus, 
        IL_DATA_SELECT_GPIO_Port, 
        IL_DATA_SELECT_Pin,
        SPI1_CS_IL_GPIO_Port,
        SPI1_CS_IL_Pin,
        IL_RESET_GPIO_Port,
        IL_RESET_Pin
    };  
    exp->display = DISPLAY_Configure(&config);
    DISPLAY_Init(exp->display); 
    
    TouchConfig touch = 
    {
        exp->secondary_bus,
        TOUCH_CS_GPIO_Port,
        TOUCH_CS_Pin,
        TOUCH_IRQ_GPIO_Port,
        TOUCH_IRQ_Pin
    };
    exp->touch = TOUCH_Configure(&touch);
    
    Rect screen                  = {0, 0, 320, 240};
    exp->ui                      = UI_Configure(exp->display, screen, 16, 4, false);
    uint16_t colors[ColorsCount] = {0x0000, 0x5BDF, 0x0007, 0xFEE0};
    UI_SetUIColors(exp->ui, colors);
    {
        int step = UI_GetWidthInGuides(exp->ui)/5;
        {
            char value[] = "Value=0000;";
            sprintf(value, "%d;", exp->nozzle.value);
            Rect ind_frame         = {0*step, 0, 1*step, 8};
            exp->nozzle_indicator  = UI_CreateIndicator(exp->ui, 0, ind_frame, value, 0, true);
        }
        {
            char value[] = "Value=0000;";
            sprintf(value, "%d;", exp->table.value);
            Rect ind_frame          = {1*step, 0, 2*step, 8};
            exp->table_indicator    = UI_CreateIndicator(exp->ui, 0, ind_frame, value, 0, true);
        }
        
        {
            Rect ind_frame          = {2*step, 0, 3*step, 8};
            exp->sdcard_indicator   = UI_CreateIndicator(exp->ui, 0, ind_frame, "CARD", 0, exp->card_state);
        }
        
        {
            Rect ind_frame          = {3*step, 0, 4*step, 8};
            exp->ram_indicator      = UI_CreateIndicator(exp->ui, 0, ind_frame, "RAM", 0, exp->ram_state);
        }
        {
            Rect ind_frame          = {4*step, 0, 5*step, 8};
            exp->timer              = UI_CreateIndicator(exp->ui, 0, ind_frame, "TIM", 0, false);
        }
        {
            Rect main_frame = {0, 8, 60, 40};
            
            HFrame window = UI_CreateFrame(exp->ui, 0, main_frame, true);
            {
                Rect text_area = UI_GetFrameUserArea(exp->ui, window);
                DISPLAY_SetTextArea(exp->display, &text_area);
                    
                Rect frame = {60, 8, 80, UI_GetHeightInGuides(exp->ui)};
                HFrame buttons = UI_CreateFrame(exp->ui, 0, frame, false);
                {
                    Rect frame = {0, 0, 10, 10};
                    UI_CreateButton(exp->ui, buttons, frame, "+100", true, add100, exp);
                }
                {
                    Rect frame = {0, 10, 10, 20};
                    UI_CreateButton(exp->ui, buttons, frame, "+50", true, add50, exp);
                }
                {
                    Rect frame = {0, 20, 10, 30};
                    UI_CreateButton(exp->ui, buttons, frame, "+10", true, add10, exp);
                }
                {
                    Rect frame = {0, 30, 10, 40};
                    UI_CreateButton(exp->ui, buttons, frame, "-10", true, remove100, exp);
                }
                {
                    Rect frame = {10, 0, 20, 20};
                    UI_CreateButton(exp->ui, buttons, frame, "+100", true, tableRemove100, exp);
                }
                {
                    Rect frame = {10, 20, 20, 40};
                    UI_CreateButton(exp->ui, buttons, frame, "-100", true, tableAdd100, exp);
                }
                {
                    Rect frame = {0, 40, 20, 50};
                    UI_CreateButton(exp->ui, buttons, frame, "Reset", true, reset, exp);
                }
            }
        }
        {
            Rect main_frame = {0, 40, 60, 60};
        
            HFrame window = UI_CreateFrame(exp->ui, 0, main_frame, false);
            {
                Rect frame = {0, 0, 10, 10};
                UI_CreateButton(exp->ui, window, frame, "E", true, calibrate_engine, &exp->e);
            }
            {
                Rect frame = {0, 10, 10, 20};
                UI_CreateButton(exp->ui, window, frame, "+E-", true, dir_engine, &exp->e);
            }
            {
                Rect frame = {10, 0, 20, 10};
                UI_CreateButton(exp->ui, window, frame, "X", true, run_engine, &exp->x);
            }
            {
                Rect frame = {10, 10, 20, 20};
                UI_CreateButton(exp->ui, window, frame, "+X-", true, dir_engine, &exp->x);
            }
            {
                Rect frame = {20, 0, 30, 10};
                UI_CreateButton(exp->ui, window, frame, "Y", true, run_engine, &exp->y);
            }
            {
                Rect frame = {20, 10, 30, 20};
                UI_CreateButton(exp->ui, window, frame, "+Y-", true, dir_engine, &exp->y);
            }
            {
                Rect frame = {30, 0, 40, 10};
                UI_CreateButton(exp->ui, window, frame, "Z", true, run_engine, &exp->z);
            }
            {
                Rect frame = {30, 10, 40, 20};
                UI_CreateButton(exp->ui, window, frame, "+Z-", true, dir_engine, &exp->z);
            }
            //{
            //    Rect frame = {40, 0, 50, 20};
            //    UI_CreateButton(exp->ui, window, frame, "CLR", false, toggle_nozzle_cooler, 0);
            //}
            {
                Rect frame = {50, 0, 60, 10};
                UI_CreateButton(exp->ui, window, frame, "SAV", true, savePrinterState, exp);
            }
            {
                Rect frame = {50, 10, 60, 20};
                UI_CreateButton(exp->ui, window, frame, "LAD", true, restorePrinterState, exp);
            }
        }
    }
    
    EqualizerConfig eq_config = {
        exp->nozzle.value,
        onHeat,
        onCool,
        onTemperatureReached,
        (void*)exp
    };
    exp->nozzle.equlizer = EQ_Configure(&eq_config);
    
    EqualizerConfig table_eq_config = {
        exp->table.value,
        onTableHeat,
        onTableCool,
        onTemperatureReached,
        (void*)exp
    };
    exp->table.equlizer = EQ_Configure(&table_eq_config);
    
    GCodeAxisConfig cfg = {100, 100, 100, 100};
    exp->gcode = GC_Configure(&cfg);
    GC_ParseCommand(exp->gcode, "G0 X100 Y20 F6400 Z0.1");
    uint8_t buffer[GCODE_CHUNK_SIZE];
    GC_CompressCommand(exp->gcode, buffer);
    GC_ExecuteFromBuffer(0, buffer);
    
    exp->timer_value = 0;
    exp->timer_updated = false;
    
    exp->initialized = true;
	
	return (HEXPERIMENTAL)exp;
}

void EXPERIMENTAL_Reset(HEXPERIMENTAL experimental)
{
    Experimental* exp = (Experimental*)experimental;
    exp->text_place.x0 = 100;
    exp->text_place.y0 = 120;
    exp->text_place.x1 = 220;
    exp->text_place.y1 = 150;
    
    exp->status_bar.x0 = 10;
    exp->status_bar.y0 = 200;
    exp->status_bar.x1 = 310;
    exp->status_bar.y1 = 230;
    
    DISPLAY_SetBackgroundColor(exp->display, 0x0007);
    DISPLAY_Cls(exp->display);
    
    Rect screen = {0, 0, 320, 240};
    DISPLAY_FillRect(exp->display, screen, 0x0);
    UI_Refresh(exp->ui);
    
    SDCARD_FAT_Register(exp->card, 0);
    
    if (SDCARD_OK == SDCARD_FAT_IsInitialized(0))
    {
        WritePrinterStatus(exp, "Printer ready");
    }
    else
    {
        PrintStatusMessage(exp->display, ES_FAIL, "FAT");
    }
    // e engine reversed Setstate is down
    dir_engine(&exp->e);
    exp->timer_value = 0;
    UI_SetIndicatorValue(exp->ui, exp->timer, false);
    
    restorePrinterState(exp);
}

static uint16_t UpdateTermalSensor(Experimental* exp, TermalSensor* sensor)
{
    uint16_t voltage = 0;
    for (int i = 0; i < 15; ++i)
    {
        voltage += sensor->voltage[i];
    }
    voltage /= 15;
    EQ_HandleTick(sensor->equlizer, voltage);
    return voltage;
}

void EXPERIMENTAL_MainLoop(HEXPERIMENTAL experimental)
{
	Experimental* exp = (Experimental*)experimental;
    char name[32];
    
    uint16_t voltage = UpdateTermalSensor(exp, &exp->nozzle);
    sprintf(name, "%d", voltage);
    UI_SetIndicatorLabel(exp->ui, exp->nozzle_indicator, name);
    
    voltage = UpdateTermalSensor(exp, &exp->table);
    sprintf(name, "%d", voltage);
    UI_SetIndicatorLabel(exp->ui, exp->table_indicator, name);
    
    if (exp->timer_updated)
    {
        UI_SetIndicatorValue(exp->ui, exp->timer, true);
        sprintf(name, "%d", exp->timer_value);
        UI_SetIndicatorLabel(exp->ui, exp->timer, name);
        exp->timer_updated = false;
    }
    
    // card
    bool sdcard_state = (SDCARD_OK == SDCARD_IsInitialized(exp->card));
    if (exp->card_state != sdcard_state)
    {
        exp->card_state = sdcard_state;
        UI_SetIndicatorValue(exp->ui, exp->sdcard_indicator, exp->card_state);
    }
    
    if (!exp->card_state)
    {
        SDCARD_Init(exp->card);
        SDCARD_ReadBlocksNumber(exp->card);
    }
    
    // RAM
    bool ram_state = (SDCARD_OK == SDCARD_IsInitialized(exp->ram));
    if (exp->ram_state != ram_state)
    {
        exp->ram_state = ram_state;
        UI_SetIndicatorValue(exp->ui, exp->ram_indicator, exp->ram_state);
    }
    
    if (!exp->ram_state)
    {
        SDCARD_Init(exp->ram);
        SDCARD_ReadBlocksNumber(exp->ram);
    }
    
    if (exp->x.status)
    {
        resetEngine(exp, &exp->x, 'x');
    }
    
    if (exp->y.status)
    {
        resetEngine(exp, &exp->y, 'y');
    }
    
    if (exp->z.status)
    {
        resetEngine(exp, &exp->z, 'z');
    }
    
    if (exp->e.status)
    {
        resetEngine(exp, &exp->e, 'e');
    }
        
    if (TOUCH_Pressed(exp->touch))
    {
        uint16_t x = TOUCH_GetX(exp->touch);
        uint16_t y = TOUCH_GetY(exp->touch);
        if (x!= 0xFFFF && y != 0xFFFF)
        {
            UI_TrackTouchAction(exp->ui, x, y, true);
        }
    }
    else
    {
        UI_TrackTouchAction(exp->ui, 0, 0, false);
    }
}

static void checkTermalSensor(TermalSensor* sensor, uint16_t value)
{
    sensor->voltage[sensor->step++] = value;
    if (sensor->step == 15)
    {
        sensor->step = 0;
    }
}

void EXPERIMENTAL_CheckTemperature(HEXPERIMENTAL experimental, ADC_HandleTypeDef* hadc)
{
    Experimental* exp = (Experimental*)experimental;
    checkTermalSensor(&exp->nozzle, HAL_ADC_GetValue(hadc));
    HAL_ADC_Stop(hadc);
}
 
void EXPERIMENTAL_CheckTableTemperature(HEXPERIMENTAL experimental, ADC_HandleTypeDef* hadc)
{
    Experimental* exp = (Experimental*)experimental;
    checkTermalSensor(&exp->table, HAL_ADC_GetValue(hadc));
    HAL_ADC_Stop(hadc);
}
 
void EXPERIMENTAL_OnTimer(HEXPERIMENTAL experimental)
{
    
    // 96MHz timer, PSC = 95, period = 100 Freq = (96 000 000) / ((95+1)*100) = 10000 
    // Tick = 0.1 mSec
    static uint32_t tick = 0;
    tick++;
    Experimental* exp = (Experimental*)experimental;
    
    if (!exp->initialized)
    {
        return;
    }
    
    if ((0 == tick % 10000) && (exp->timer_value > 0))
    {
        --exp->timer_value;
        exp->timer_updated = true;
    }
    
    if (exp->e.running && 0 == tick % exp->e.speed)
    {
        if (exp->e.steps > 0)
        {
            HAL_GPIO_WritePin(E_ENG_STEP_GPIO_Port, E_ENG_STEP_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(E_ENG_STEP_GPIO_Port, E_ENG_STEP_Pin, GPIO_PIN_RESET);
            --exp->e.steps;
        }
    }
    
    if (exp->x.running && 0 == tick % exp->x.current_speed)
    {
        --exp->x.steps;
        if (exp->x.steps < 500)
        {
            exp->x.current_speed = exp->x.speed + (500 - exp->x.steps)*(10-exp->x.speed)/500;
        }
        if (!exp->x.steps)
        {
            exp->x.running = false;
            exp->x.status = true;
        }
        HAL_GPIO_WritePin(exp->x.step_port, exp->x.step_pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(exp->x.step_port, exp->x.step_pin, GPIO_PIN_RESET);
    }
    
    if (exp->y.running && 0 == tick % exp->y.current_speed)
    {
        --exp->y.steps;
        if (exp->y.steps < 500)
        {
            exp->y.current_speed = exp->y.speed + (500 - exp->y.steps)*(10-exp->y.speed)/500;
        }
        
        if (!exp->y.steps)
        {
            exp->y.running = false;
            exp->y.status = true;
        }
        HAL_GPIO_WritePin(exp->y.step_port, exp->y.step_pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(exp->y.step_port, exp->y.step_pin, GPIO_PIN_RESET);
    }
    
    if (exp->z.running && 0 == tick % exp->z.speed)
    {
        --exp->z.steps;
        if (!exp->z.steps)
        {
            exp->z.running = false;
            exp->z.status = true;
        }
        HAL_GPIO_WritePin(exp->z.step_port, exp->z.step_pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(exp->z.step_port, exp->z.step_pin, GPIO_PIN_RESET);
    }
}
