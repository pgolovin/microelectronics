#include "main.h"
#include "experimental.h"

#include "include/spibus.h"
#include "include/sdcard.h"
#include "include/display.h"
#include "include/touch.h"
#include "include/user_interface.h"

#include "ff.h"
#include "diskio.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
    HSPIBUS main_bus;
    HSPIBUS slow_bus;
	HSDCARD sdcard;
    bool sdcard_ready;
    HDISPLAY display;
    UI ui;
    HIndicator sdcard_indicator;
    HIndicator touch_indicator;
    HIndicator error_indicator;
    HIndicator engine_indicator;

    HButton    btn_engine;
    HButton    rnd_engine;
    HButton    sd_action;
    HTOUCH touch;
    
    Rect steps_counter;
    
    FATFS fs;
    bool logfile_ready;
    bool touched;
    bool rotation_enabled;
    bool rotation_cw;
    uint32_t rotation_speed;
    uint32_t rotation_step;
    uint32_t total_steps;
    uint32_t steps_per_random_section;
	
} Experimental;

enum EXECUTION_STATUS
{
    ES_OK,
    ES_WARNING,
    ES_FAIL,
};

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

void WriteLogFile(Experimental* exp)
{
    FATFS* fat_fs = &exp->fs;
	FRESULT result;
	result = f_mount(&exp->fs, "", 0);
	uint32_t sectors = 0;
	f_getfree("", &sectors, &fat_fs);
	
	FIL log_file;
	result = f_open(&log_file, "log.txt", FA_OPEN_APPEND | FA_WRITE);
    exp->logfile_ready = (result == RES_OK);
    
 	if (result == RES_OK)
	{
        PrintStatusMessage(exp->display, ES_OK, "Log file openned");
        
        f_printf(&log_file, "Loaded\n");
        for (uint8_t i = 0; i < 128; ++i)
        {
            f_printf(&log_file, "%c %d;\n", i, i);
        }
		f_close(&log_file);
	}
    
    UI_SetIndicatorValue(exp->ui, exp->error_indicator, result != RES_OK);
	
	f_mount(0, "", 0);
}

bool onCW(void* metadata)
{
    Experimental* exp = (Experimental*)metadata;
    
    PrintStatusMessage(exp->display, ES_OK, "Clock-wise");
    HAL_GPIO_WritePin(ENG1_Direction_GPIO_Port, ENG1_Direction_Pin, GPIO_PIN_RESET);
    
    return true;
}

bool onCCW(void* metadata)
{
    Experimental* exp = (Experimental*)metadata;
    
    PrintStatusMessage(exp->display, ES_OK, "Counter Clock-wise");
    HAL_GPIO_WritePin(ENG1_Direction_GPIO_Port, ENG1_Direction_Pin, GPIO_PIN_SET);
    
    return true;
}

bool onRandom(void* metadata)
{
    Experimental* exp = (Experimental*)metadata;
    if (exp->steps_per_random_section > 10000)
    {
        exp->steps_per_random_section = rand()%20*100;
        UI_SetIndicatorLabel(exp->ui, exp->engine_indicator, "RND!");
    }
    else
    {
        exp->steps_per_random_section = 0xFFFFFFFF;
        UI_SetIndicatorLabel(exp->ui, exp->engine_indicator, "ENG");
    }
    return true;
}

bool onToggleStart(void* metadata)
{
    Experimental* exp = (Experimental*)metadata;
    
    exp->rotation_enabled = !exp->rotation_enabled;
    UI_SetIndicatorValue(exp->ui, exp->engine_indicator, exp->rotation_enabled);
    
    if (exp->rotation_enabled)
    {
        PrintStatusMessage(exp->display, ES_OK, "Engine Started");
        UI_SetButtonLabel(exp->ui, exp->btn_engine, "STOP");
    }
    else
    {
        PrintStatusMessage(exp->display, ES_WARNING, "Engine Stopped");
        UI_SetButtonLabel(exp->ui, exp->btn_engine, "START");
    }
    UI_EnableButton(exp->ui, exp->rnd_engine, exp->rotation_enabled);
    
    return true;
}

bool DumpScreenContent(void* metadata)
{
    Experimental* exp = (Experimental*)metadata;
    
    FATFS* fat_fs = &exp->fs;
	FRESULT result;
	result = f_mount(&exp->fs, "", 0);
	uint32_t sectors = 0;
	f_getfree("", &sectors, &fat_fs);
	
	FIL log_file;
	result = f_open(&log_file, "log.txt", FA_OPEN_APPEND | FA_WRITE);
    exp->logfile_ready = (result == RES_OK);
    
 	if (result == RES_OK)
	{
        PrintStatusMessage(exp->display, ES_OK, "Log file openned");
        
        f_printf(&log_file, "DumpScreenContent\n");
        for (uint8_t i = 0; i < 128; ++i)
        {
            f_printf(&log_file, "%c %d;\n", i, i);
        }
		f_close(&log_file);
        PrintStatusMessage(exp->display, ES_OK, "Log file ready");
	}
    
    UI_SetIndicatorValue(exp->ui, exp->error_indicator, result != RES_OK);
	
	f_mount(0, "", 0);
    
    return true;
}

bool onButtonClearScreen(void* metadata)
{
    Experimental* exp = (Experimental*)metadata;
    
    DISPLAY_Cls(exp->display);
    
    return true;
}

HEXPERIMENTAL EXPERIMENTAL_Configure(SPI_HandleTypeDef* hspi, SPI_HandleTypeDef* hslow_spi)
{
	Experimental* exp = malloc(sizeof(Experimental));
	
    exp->main_bus = SPIBUS_Configure(hspi, HAL_MAX_DELAY);
    exp->slow_bus = SPIBUS_Configure(hslow_spi, HAL_MAX_DELAY);
    exp->rotation_cw = false;
    exp->rotation_enabled = false;
    exp->total_steps = 0;
    exp->rotation_speed = 1;
    exp->rotation_step = 0;
    exp->steps_per_random_section = 0xFFFFFFFF;
    
    DisplayConfig config = 
    {
        exp->main_bus, 
        SPI1_DC_Secondary_GPIO_Port, 
        SPI1_DC_Secondary_Pin,
        SPI1_CS_Secondary_GPIO_Port,
        SPI1_CS_Secondary_Pin,
        SPI1_LED_Reset_GPIO_Port,
        SPI1_LED_Reset_Pin
    };  
    exp->sdcard = SDCARD_Configure(exp->main_bus, SPI1_CS_Primary_GPIO_Port, SPI1_CS_Primary_Pin);

    TouchConfig t_cfg = 
    {
        exp->slow_bus,
        SPI2_CS_Primary_GPIO_Port,
        SPI2_CS_Primary_Pin,
        
        SPI2_IRQ_Primary_GPIO_Port,
        SPI2_IRQ_Primary_Pin,
    };
    exp->touch = TOUCH_Configure(&t_cfg);
    exp->touched = false;
    
    SDCARD_Init(exp->sdcard);
    SDCARD_GetBlocksNumber(exp->sdcard);
    exp->sdcard_ready = (SDCARD_OK == SDCARD_IsInitialized(exp->sdcard));
    
    exp->display = DISPLAY_Configure(&config);
    DISPLAY_Init(exp->display); 
    Rect screen = {0, 0, 320, 240};
    DISPLAY_FillRect(exp->display, screen, 0x0);
    
    // Configure UI
    exp->ui = UI_Configure(exp->display, screen, 16, 4, false);
    uint16_t colors[ColorsCount] = {0x0000, 0x5BDF, 0x0007, 0xFEE0};
    UI_SetUIColors(exp->ui, colors);
    {
        Rect frame = {0, 8, 60, 60};
        HFrame window = UI_CreateFrame(exp->ui, 0, frame, true);
        
        Rect text_area = UI_GetFrameUserArea(exp->ui, window);
        exp->steps_counter = text_area;
        text_area.y1 -= 16;
        
        DISPLAY_SetTextArea(exp->display, &text_area);
        exp->steps_counter.y0 = exp->steps_counter.y1 - 8;
    }
    {
        Rect frame = {0, 0, UI_GetWidthInGuides(exp->ui)/5, 8};
        UI_CreateIndicator(exp->ui, 0, frame, "DSPL", 0, true);
    }
    {
        Rect frame = {UI_GetWidthInGuides(exp->ui)/5, 0, UI_GetWidthInGuides(exp->ui)/5 * 2, 8};
        exp->sdcard_indicator = UI_CreateIndicator(exp->ui, 0, frame, "SD/MMC", 0, exp->sdcard_ready);
    }
    {
        Rect frame = {UI_GetWidthInGuides(exp->ui)/5 * 2, 0, UI_GetWidthInGuides(exp->ui)/5 * 3, 8};
        exp->engine_indicator = UI_CreateIndicator(exp->ui, 0, frame, "ENG", 0, false);
    }
    {
        Rect frame = {UI_GetWidthInGuides(exp->ui)/5 * 3, 0, UI_GetWidthInGuides(exp->ui)/5 * 4, 8};
        exp->touch_indicator = UI_CreateIndicator(exp->ui, 0, frame, "TOUCH", 0, false);
    }
    {
        Rect frame = {UI_GetWidthInGuides(exp->ui)/5 * 4, 0, UI_GetWidthInGuides(exp->ui)/5 * 5, 8};
        exp->error_indicator = UI_CreateIndicator(exp->ui, 0, frame, "ERROR", 0xF9A6, false);
    }
    {
        Rect frame = {60, 8, 80, UI_GetHeightInGuides(exp->ui)};
        HFrame buttons = UI_CreateFrame(exp->ui, 0, frame, false);
        int step = (UI_GetHeightInGuides(exp->ui) - 8)/5;
        {
            Rect frame = {0, 0, 20, step};
            exp->btn_engine = UI_CreateButton(exp->ui, buttons, frame, "START", true, onToggleStart, exp);
        }
        {
            Rect frame = {0, step, 10, step*2};
            UI_CreateButton(exp->ui, buttons, frame, "CW", true, onCW, exp);
        }
        {
            Rect frame = {10, step, 20, step*2};
            UI_CreateButton(exp->ui, buttons, frame, "CCW", true, onCCW, exp);
        }
        
        {
            Rect frame = {0, step*2, 20, step*3};
            exp->rnd_engine = UI_CreateButton(exp->ui, buttons, frame, "RND", false, onRandom, exp);
        }
        {
            Rect frame = {0, step*3, 20, step*4};
            exp->sd_action = UI_CreateButton(exp->ui, buttons, frame, "LOG", exp->sdcard_ready, DumpScreenContent, exp);
        }
        {
            Rect frame = {0, step*4, 20, step*5};
            UI_CreateButton(exp->ui, buttons, frame, "Clear", true, onButtonClearScreen, exp);
        }
    }    
    UI_Refresh(exp->ui);
    
    PrintStatusMessage(exp->display, ES_OK, "Display ready");
    
    if (exp->sdcard_ready)
    {
        PrintStatusMessage(exp->display, ES_OK, "SD card ready");
    }
    else 
    {
        PrintStatusMessage(exp->display, ES_FAIL, "SD card not found");
    }
    		
	SDCARD_FAT_Register(exp->sdcard, 0);
	
    WriteLogFile(exp);    
	
	return (HEXPERIMENTAL)exp;
}
void EXPERIMENTAL_MainLoop(HEXPERIMENTAL experimental)
{
	Experimental* exp = (Experimental*)experimental;
    bool sdcard_state = (SDCARD_OK == SDCARD_IsInitialized(exp->sdcard));
	if (sdcard_state != exp->sdcard_ready)
    {
        exp->sdcard_ready = sdcard_state;
        
        if (exp->sdcard_ready)
        {
            PrintStatusMessage(exp->display, ES_OK, "SD card connected");
        }
        else 
        {
            PrintStatusMessage(exp->display, ES_WARNING, "SD card disconnected");
        }
    }
    
    if (!exp->sdcard_ready)
    {
        SDCARD_Init(exp->sdcard);
        SDCARD_GetBlocksNumber(exp->sdcard);
        exp->sdcard_ready = (SDCARD_OK == SDCARD_IsInitialized(exp->sdcard));
        
        if (exp->sdcard_ready)
        {
            PrintStatusMessage(exp->display, ES_OK, "SD card ready");
            if (!exp->logfile_ready)
            {
                PrintStatusMessage(exp->display, ES_WARNING, "Log file was not created. Creating");
                WriteLogFile(exp);
            }
        }
    }
    
    bool touched = TOUCH_Pressed(exp->touch);
    UI_SetIndicatorValue(exp->ui, exp->sdcard_indicator, exp->sdcard_ready);
    UI_SetIndicatorValue(exp->ui, exp->touch_indicator, touched);
    UI_EnableButton(exp->ui, exp->sd_action, exp->sdcard_ready);
    char name[32];
    
    sprintf(name, "Steps: %u", exp->total_steps);
    DISPLAY_DrawString(exp->display, &exp->steps_counter, name);
    
    uint16_t x = 0;
    uint16_t y = 0;
    
    if (touched)
    {
        x = TOUCH_GetX(exp->touch);
        y = TOUCH_GetY(exp->touch);
    }
    UI_TrackTouchAction(exp->ui, x, y, touched);
    
    if (0 == exp->steps_per_random_section)
    {
        char random_info[32] = "";
        int direction = rand()%2;
        
        exp->rotation_speed = rand()%20 + 1;
        exp->rotation_step = 0;
        exp->steps_per_random_section = ((rand()%50 + 10) * 250)/exp->rotation_speed;
        
        HAL_GPIO_WritePin(ENG1_Direction_GPIO_Port, ENG1_Direction_Pin, direction ? GPIO_PIN_SET : GPIO_PIN_RESET);
        
        sprintf(random_info, "R=%s, V=1/%u, L=%u", direction ? "CCW" : "CW", exp->rotation_speed, exp->steps_per_random_section);
        PrintStatusMessage(exp->display, ES_WARNING, random_info);
    }
}

void EXPERIMENTAL_OnTimer(HEXPERIMENTAL experimental)
{
    Experimental* exp = (Experimental*)experimental;
    if (exp->rotation_enabled && exp->steps_per_random_section > 0)
    {
        ++exp->rotation_step;
        if (exp->rotation_step >= exp->rotation_speed)
        {
            HAL_GPIO_WritePin(ENG1_Step_GPIO_Port, ENG1_Step_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(ENG1_Step_GPIO_Port, ENG1_Step_Pin, GPIO_PIN_RESET);
            ++exp->total_steps;
            exp->rotation_step = 0;
            --exp->steps_per_random_section;
        }
    }
}
