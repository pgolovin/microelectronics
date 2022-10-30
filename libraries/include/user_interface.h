#include "main.h"

#ifndef _WIN32
#include "include/display.h"
#else
#include "display.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __SIMPLE_UI__
#define __SIMPLE_UI__

#define NORMAL_FONT 8
#define LARGE_FONT 16

typedef struct UI_type
{
    uint32_t id;
} *UI;

struct Frame_type
{
    uint32_t id;
};
typedef struct Frame_type * HFrame;

typedef struct
{
    uint32_t id;
}* HLabel;

struct Indicator_type
{
    uint32_t id;
};
typedef struct Indicator_type * HIndicator;

struct Button_type
{
    uint32_t id;
};
typedef struct Button_type * HButton;

typedef struct
{
    void* metadata;
    void* subparameter;
} ActionParameter;

typedef bool(*Action)(ActionParameter* action_data);

// Configure UI 
// setup the display, screen area and main size unit of the interface "module"
// recommended size of the module 16 equals to the font size
// "guide" is subunit required to create user interface layout
// recomended size of guide is 4, in defult case 4 guides per module
UI UI_Configure(HDISPLAY context, Rect viewport, uint8_t module_size, uint8_t guides_per_module, bool root_visible);

// Return size of the screen in guides
uint16_t UI_GetHeightInGuides(UI ui_handle);
uint16_t UI_GetWidthInGuides(UI ui_handle);

// Setup color schema. currently only 5 colors are supported
//  - background       - just a screen background
//  - main color       - color for outlines, indication of button click action, and default text color
//  - secondary color  - color for in-button backgrounds and indication of disabled item
//  - indicator colors - indicator colors, on

enum ColorSchemaIndexes
{
    ColorBackground = 0,
    ColorMain,
    ColorSecondary,
    ColorIndicator,
    //----//
    ColorsCount,
};

void UI_SetUIColors(UI ui_handle, uint16_t colors[ColorsCount]);

// UI construction.
// WARNING! ALL SIZES HERE ARE IN GUIDES, NOT PIXELS
// Coordinates are local to parent

// base element is Frame, everything should be placed on the frame
// it can be visible to indicate group and invisible to maintain hierarchy
HFrame     UI_GetRootFrame(UI ui_handle);

// All UI elements can be created only on the frame. if frame is not defined, UI item will not be created
HFrame     UI_CreateFrame(UI ui_handle, HFrame parent, Rect frame, bool visible);
HLabel     UI_CreateLabel(UI ui_handle, HFrame parent, Rect frame, const char* label, uint8_t font_height);
HButton    UI_CreateButton(UI ui_handle, HFrame parent, Rect button_rect, const char* label, uint8_t font_height, bool enabled, Action action, void* metadata, void* subparameter);
HIndicator UI_CreateIndicator(UI ui_handle, HFrame parent, Rect indicator_rect, const char* label, uint8_t font_height, uint16_t custom_color, bool default_state);

Rect       UI_GetFrameUserArea(UI ui_handle, HFrame frame);
void       UI_EnableButton(HButton hbutton, bool enabled);
void       UI_SetButtonLabel(HButton hbutton, const char* label);
void       UI_SetLabel(HLabel hlabel, const char* label);
void       UI_EnableFrame(HFrame hframe, bool enabled);
void       UI_Print(UI ui_handle, const Rect* area, const char* label);

// UI items mahagement
void UI_SetIndicatorValue(HIndicator indicator, bool state);
void UI_SetIndicatorLabel(HIndicator indicator, const char* label);

void UI_Refresh(UI ui_handle);

// Touch management
void UI_TrackTouchAction(UI ui_handle, uint16_t x, uint16_t y, bool touch);


#endif //__SIMPLE_UI__

#ifdef __cplusplus
}
#endif
