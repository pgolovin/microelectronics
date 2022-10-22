#ifndef __UI_ITEMS__
#define __UI_ITEMS__

// 2bit per pixel indexed color
/*
 frame area
 4x4  corners 8x4 middle line 4x4 corner 
 4x1  edge    8x1 middle line 4x1 edge
 4x4  corners 8x4 middle line 4x4 corner 
*/
int ui_item_frame[] = 
{
    0x00000000, 
    0x00000000, 
    0x01555540, 
    0x05AAAA50, 
    0x06AAAA90, 
    0x05AAAA50,
    0x01555540,
    0x00000000,
    0x00000000,
};
// frame 16x9

int ui_item_indicator[] = 
{
    0x00000000, 
    0x00000000,
    0x00000000, 
    0x00000000,
    0x00000000, 
    0xA7DAA7DA, 
    0x57D557D5,
    0x00000000, 
    0x00000000,
};

int ui_item_label[] =
{
    0x05AAAA50,
    0x06AAAA90,
    0x06AAAA90,
    0x06AAAA90,
    0x06AAAA90,
    0x06AAAA90,
    0x06AAAA90,
    0x06AAAA90,
    0x05AAAA50,
};
// indicator 16x9

#endif //__UI_ITEMS__
