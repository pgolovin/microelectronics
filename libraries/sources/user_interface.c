#include "main.h"
#include "include/user_interface.h"
#include "misc/interface_items.h"
#include "misc/font8x8.h"
#include <stdlib.h>
#include <string.h>

#define BORDER_OFFSET   4
#define LABEL_LENGTH    16
#define CHILDREN_COUNT  10

#define LETTER_HEIGHT   16
#define LETTER_WIDTH    8

#pragma pack(push, 1)
enum UIItemTypes
{
    UIFrame,
    UIButton,
    UIIndicator,
};

typedef struct
{
    uint8_t type;
    void*   handle;
} UIItem;

typedef struct
{
    Rect    frame;
    char    label[LABEL_LENGTH];
    uint16_t color;
    bool    state;
} Indicator;

typedef struct
{
    Rect    frame;
    char    label[LABEL_LENGTH];
    bool    enabled;
    void*   metadata;
    Action  action;
} Button;

typedef struct Internal_Frame_type
{
    struct Internal_Frame_type* parent;
    
    Rect        frame;
    bool        visible;
    const char* text;
    // yeah, artifitial limit, but lets try not to make uber interfaces
    UIItem      children[CHILDREN_COUNT];
    uint8_t     count;
} Frame;

typedef struct 
{
    HDISPLAY context;
    Frame    root;
    uint8_t  module;
    uint8_t  guide;
    uint16_t color_schema[ColorsCount];
    Button*  focused_item;
} UI_CORE;

#pragma pack(pop)

static void DrawRect(UI_CORE* ui, const Rect* frame, const char* label, bool state, bool enabled, bool focused, const int* texture, uint16_t text_color)
{
    //TODO collapse the function
    DISPLAY_BeginDraw(ui->context, *frame);
    
    uint16_t color_schema[ColorsCount] = {
        ui->color_schema[ColorBackground],
        ui->color_schema[ColorMain],
        ui->color_schema[ColorSecondary],
        ui->color_schema[ColorIndicator],
    };
    if (!enabled)
    {
        color_schema[ColorMain] = ui->color_schema[ColorSecondary];
        color_schema[ColorSecondary] = ui->color_schema[ColorBackground];
    }
    if (focused)
    {
        text_color = ui->color_schema[ColorSecondary];
        color_schema[ColorSecondary] = ui->color_schema[ColorMain];
    }
    if (!state || !enabled)
    {
        text_color = color_schema[ColorSecondary];
        color_schema[ColorIndicator] = color_schema[ColorMain];
    }
    
    uint16_t scan_line[320] = {0};
    
    uint16_t line = (frame->x1 - frame->x0);
    uint16_t raws = (frame->y1 - frame->y0);
    uint8_t texture_line = 0;
    uint8_t texture_raw = 0;
    
    uint16_t label_length = strlen(label)*LETTER_WIDTH;
    int label_position_x = (line - label_length) / 2;
    if (label_position_x < 0)
    {
        label_position_x = 0;
        label_length = line;
    }
    
    int label_position_y = (raws - LETTER_HEIGHT) / 2;
    
    // scale texture to actual button size, keeping only border size intact
    // other sizes should be proportionally scaled
    // keep in mind that line 5 on frame texture represents middle line
    // and should be repeted all along the texture until the bottom line
    // the same is applicable for bits from 10-22, they are hase the same values
    // cos they are represent the vertical middle line and should be repeated
    
    // draw texture before label
    for (uint16_t y = 0; y < label_position_y; ++y)
    {
        // draw texture line by line
        texture_raw = 0;
        for (uint16_t x = 0; x < line; ++x)
        {
            scan_line[x] = color_schema[(texture[texture_line] >> (32 - ((texture_raw + 1)*2))) & 0x3];
            
            // increment texture X pixels only for left and right borders
            if (x < 8 || x > (line - 9))
            {
                ++texture_raw;
            }
        }
        // increment texture Y pixels for top/bottom borders
        if (y < 4 || y > raws - 6)
        {
            ++texture_line;
        }
        
        DISPLAY_WritePixels(ui->context, scan_line, line);
    }  
    // Draw label
    for (uint16_t y = 0; y < LETTER_HEIGHT; ++y)
    {
        // draw texture line by line
        texture_raw = 0;
        // draw before label
        for (uint16_t x = 0; x < label_position_x; ++x)
        {
            scan_line[x] = color_schema[(texture[texture_line] >> (32 - ((texture_raw + 1)*2))) & 0x3];
            // increment texture X pixels only for left and right borders
            if (x < 8 || x > (line - 9))
            {
                ++texture_raw;
            }
        }
        // draw before label
        for (uint16_t x = 0; x < label_length; ++x)
        {
            scan_line[label_position_x + x] = color_schema[(texture[texture_line] >> (32 - ((texture_raw + 1)*2))) & 0x3];
            
            char c = label[x / LETTER_WIDTH];
            if ((s_font_8x8[c - 32][y * 8 / LETTER_HEIGHT] >> (x % 8)) & 0x1)
            {
                scan_line[label_position_x + x] = text_color;
            }
            
            // increment texture X pixels only for left and right borders
            if (label_position_x + x < 8 || label_position_x + x > (line - 9))
            {
                ++texture_raw;
            }
        }
        // draw after label
        for (uint16_t x = label_position_x + label_length; x < line; ++x)
        {
            scan_line[x] = color_schema[(texture[texture_line] >> (32 - ((texture_raw + 1)*2))) & 0x3];
            
            // increment texture X pixels only for left and right borders
            if (x < 8 || x > (line - 9))
            {
                ++texture_raw;
            }
        }
        // increment texture Y pixels for top/bottom borders
        if (y+label_position_y < 4 || y+label_position_y > raws - 6)
        {
            ++texture_line;
        }
        
        DISPLAY_WritePixels(ui->context, scan_line, line);
    } 
    
    for (uint16_t y = label_position_y + LETTER_HEIGHT; y < raws; ++y)
    {
        // draw texture line by line
        texture_raw = 0;
        for (uint16_t x = 0; x < line; ++x)
        {
            scan_line[x] = color_schema[(texture[texture_line] >> (32 - ((texture_raw + 1)*2))) & 0x3];
            
            // increment texture X pixels only for left and right borders
            if (x < 8 || x > (line - 9))
            {
                ++texture_raw;
            }
        }
        // increment texture Y pixels for top/bottom borders
        if (y < 4 || y > raws - 6)
        {
            ++texture_line;
        }
        
        DISPLAY_WritePixels(ui->context, scan_line, line);
    }
    
    DISPLAY_EndDraw(ui->context);
}

static void DrawButton(UI_CORE* ui, Button* frame)
{
    DrawRect(ui, &frame->frame, frame->label, true, frame->enabled, frame == ui->focused_item, ui_item_frame, ui->color_schema[ColorMain]);
}

static void DrawIndicator(UI_CORE* ui, Indicator* indicator)
{
    DrawRect(ui, &indicator->frame, indicator->label, indicator->state, true, false, ui_item_indicator, indicator->color);
}

static void DrawFrame(UI_CORE* ui, Frame* frame)
{
    if (frame->visible)
    {
        DrawRect(ui, &frame->frame, "", false, true, false, ui_item_frame, ui->color_schema[ColorMain]);
    }
    for (int child = 0; child < frame->count; ++child)
    {
        if (UIIndicator == frame->children[child].type)
        {
            DrawIndicator(ui, frame->children[child].handle);
        }
        else if (UIButton == frame->children[child].type)
        {
            DrawButton(ui, frame->children[child].handle);
        }
        else
        {
            DrawFrame(ui, frame->children[child].handle);
        }
    }
}

static void CalculateFrame(Rect* result_frame, const Rect* local_frame, const Rect* parent_frame, uint8_t guide_size)
{
    result_frame->x0 = parent_frame->x0 + local_frame->x0 * guide_size;
    result_frame->y0 = parent_frame->y0 + local_frame->y0 * guide_size;
    result_frame->x1 = result_frame->x0 + (local_frame->x1 - local_frame->x0) * guide_size;
    result_frame->y1 = result_frame->y0 + (local_frame->y1 - local_frame->y0) * guide_size;
}

static bool TrackTouch(UI_CORE* ui, UIItem* item, uint16_t x, uint16_t y)
{
    bool hit = false;
    if (item->type == UIFrame)
    {
        Frame* frame = (Frame*)(item->handle);
        hit =  x >= frame->frame.x0 
            && x <  frame->frame.x1 
            && y >= frame->frame.y0 
            && y <  frame->frame.y1;
        if (hit)
        {
            for (int c = 0; c < frame->count; ++c)
            {
                if (TrackTouch(ui, &frame->children[c], x, y))
                {
                    break;
                }
            }
        }
    }
    else if (item->type == UIButton)
    {
        Button* btn = (Button*)(item->handle);
        if (!btn->enabled)
        {
            return false;
        }
        
        hit =  x >= btn->frame.x0 
            && x <  btn->frame.x1 
            && y >= btn->frame.y0 
            && y <  btn->frame.y1;
        if (hit && ui->focused_item != btn)
        {
            if (ui->focused_item)
            {
                Button* bkp = ui->focused_item;
                ui->focused_item = 0;
                DrawButton(ui, bkp);
            }                
            ui->focused_item = btn;
            DrawButton(ui, btn);
        }
    }
    
    return hit;
}

// public functionality
UI UI_Configure(HDISPLAY context, Rect viewport, uint8_t module_size, uint8_t guides_per_module, bool root_visible)
{
    UI_CORE* ui = malloc(sizeof(UI_CORE));
    ui->context = context;
    ui->root.parent = 0; // root, no more parents
    ui->root.count = 0;
    ui->root.frame = viewport;
    ui->root.visible = root_visible;
    ui->module = module_size;
    ui->guide = module_size/guides_per_module;
    
    return (UI)ui;
}

// Return size of the screen in guides
uint16_t UI_GetHeightInGuides(UI ui_handle)
{
    UI_CORE* ui = (UI_CORE*)ui_handle;
    return (ui->root.frame.y1 - ui->root.frame.y0)/ui->guide;
}

uint16_t UI_GetWidthInGuides(UI ui_handle)
{
    UI_CORE* ui = (UI_CORE*)ui_handle;
    return (ui->root.frame.x1 - ui->root.frame.x0)/ui->guide;
}

// Setup color schema.
void UI_SetUIColors(UI ui_handle, uint16_t colors[ColorsCount])
{
    UI_CORE* ui = (UI_CORE*)ui_handle;
    for (uint8_t i = 0; i < ColorsCount; ++i)
    {
        ui->color_schema[i] = ((colors[i] >> 8) & 0xFF) | ((colors[i] & 0xFF) << 8);
    }
    
    DrawFrame(ui, &ui->root);
}

HFrame     UI_GetRootFrame(UI ui_handle)
{
    UI_CORE* ui = (UI_CORE*)ui_handle;
    return (HFrame)&ui->root;
}
// UI construction.
HFrame     UI_CreateFrame(UI ui_handle, HFrame parent, Rect frame, bool visible)
{
    UI_CORE* ui = (UI_CORE*)ui_handle;
    if (!parent)
    {
        parent = (HFrame)&ui->root;
    }
        
    Frame* parent_frame = (Frame*)parent;
    if (CHILDREN_COUNT == parent_frame->count)
    {
        return 0;
    }

    Frame* new_frame = malloc(sizeof(Frame));
    new_frame->visible = visible;    
    new_frame->parent = parent_frame;
    new_frame->count = 0;

    CalculateFrame(&new_frame->frame, &frame, &parent_frame->frame, ui->guide); 
    
    parent_frame->children[parent_frame->count].handle = new_frame;
    parent_frame->children[parent_frame->count++].type = UIFrame;
    
    return (HFrame)new_frame;
}

HButton    UI_CreateButton(UI ui_handle, HFrame parent, Rect button_rect, const char* label, bool enabled, Action action, void* metadata)
{
    UI_CORE* ui = (UI_CORE*)ui_handle;
    if (!parent)
    {
        parent = (HFrame)&ui->root;
    }
    
    Frame* parent_frame = (Frame*)parent;
    if (CHILDREN_COUNT == parent_frame->count)
    {
        return 0;
    }

    Button* button = malloc(sizeof(Button));
    size_t len = strlen(label);
    if (len > LABEL_LENGTH)
    {
        len = LABEL_LENGTH;
    }
    
    memcpy(button->label, label, len);
    button->enabled = enabled;
    button->action = action;
    button->metadata = metadata;

    CalculateFrame(&button->frame, &button_rect, &parent_frame->frame, ui->guide); 
    
    parent_frame->children[parent_frame->count].handle = button;
    parent_frame->children[parent_frame->count++].type = UIButton;
    
    return (HButton)button;
}
    
HIndicator UI_CreateIndicator(UI ui_handle, HFrame parent, Rect indicator_rect, const char* label, uint16_t custom_color, bool default_state)
{
    UI_CORE* ui = (UI_CORE*)ui_handle;
    if (!parent)
    {
        parent = (HFrame)&ui->root;
    }
    
    Frame* parent_frame = (Frame*)parent;
    if (CHILDREN_COUNT == parent_frame->count)
    {
        return 0;
    }

    Indicator* indicator = malloc(sizeof(Indicator));
    size_t len = strlen(label);
    if (len > LABEL_LENGTH)
    {
        len = LABEL_LENGTH;
    }
    
    memcpy(indicator->label, label, len);
    indicator->state = default_state;
    indicator->color = custom_color == 0 ? ui->color_schema[ColorIndicator] : ((custom_color >> 8) & 0xFF) | ((custom_color & 0xFF) << 8);

    CalculateFrame(&indicator->frame, &indicator_rect, &parent_frame->frame, ui->guide); 
    
    parent_frame->children[parent_frame->count].handle = indicator;
    parent_frame->children[parent_frame->count++].type = UIIndicator;
    
    return (HIndicator)indicator;
}

Rect UI_GetFrameUserArea(UI ui_handle, HFrame frame)
{
    Frame* ui_item = (Frame*)frame;
    
    Rect rt = 
    {
        ui_item->frame.x0 + BORDER_OFFSET, 
        ui_item->frame.y0 + BORDER_OFFSET, 
        ui_item->frame.x1 - BORDER_OFFSET, 
        ui_item->frame.y1 - BORDER_OFFSET
    };
    
    return rt;
}

void UI_EnableButton(UI ui_handle, HButton button, bool enabled)
{
    UI_CORE* ui = (UI_CORE*)ui_handle;
    Button* btn = (Button*)button;
    if (btn->enabled != enabled)
    {
        btn->enabled = enabled;
        DrawButton(ui, btn);
    }
}

void       UI_SetButtonLabel(UI ui_handle, HButton button, const char* label)
{
    UI_CORE* ui = (UI_CORE*)ui_handle;
    Button* btn = (Button*)button;
    size_t len = strlen(label);
    if (len > LABEL_LENGTH-1)
    {
        len = LABEL_LENGTH-1;
    }
    
    memcpy(btn->label, label, len);
    btn->label[len] = 0;
    DrawButton(ui, btn);
}

void       UI_SetIndicatorLabel(UI ui_handle, HIndicator indicator, const char* label)
{
    UI_CORE* ui = (UI_CORE*)ui_handle;
    Indicator* ind = (Indicator*)indicator;
    size_t len = strlen(label);
    if (len > LABEL_LENGTH-1)
    {
        len = LABEL_LENGTH-1;
    }
    
    memcpy(ind->label, label, len);
    ind->label[len] = 0;
    DrawIndicator(ui, ind);
}

// UI items mahagement
void UI_SetIndicatorValue(UI ui_handle, HIndicator indicator, bool state)
{
    UI_CORE* ui = (UI_CORE*)ui_handle;
    Indicator* ind = (Indicator*)indicator;
    if (ind->state != state)
    {
        ind->state = state;
        DrawIndicator(ui, ind);
    }
}

void UI_Refresh(UI ui_handle)
{
    UI_CORE* ui = (UI_CORE*)ui_handle;
    DrawFrame(ui, &ui->root);
}

// Touch management
void UI_TrackTouchAction(UI ui_handle, uint16_t x, uint16_t y, bool touch)
{
    UI_CORE* ui = (UI_CORE*)ui_handle;
    if (touch)
    {
        UIItem item = {UIFrame, &ui->root};
        TrackTouch(ui, &item, x, y);
    }
    else if (ui->focused_item)
    {
        Button* btn = ((Button*)ui->focused_item);
        if (btn->action && btn->enabled)
        {
            btn->action(btn->metadata);
        }
        
        ui->focused_item = 0;
        DrawButton(ui, btn);
    }
}
