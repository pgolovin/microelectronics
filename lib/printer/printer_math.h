#include "main.h"
#include "printer_entities.h"

#ifndef __PRINTER_MATH__
#define __PRINTER_MATH__

#ifdef __cplusplus
extern "C" {
#endif

uint32_t CalculateTime(const GCodeAxisConfig* axis_cfg, const GCodeCommandParams* segment);

uint32_t CalculateSegmentTime(const GCodeAxisConfig* axis_cfg, 
                                     const GCodeCommandParams* current_point, 
                                     const GCodeCommandParams* previous_point);

double Dot(const GCodeCommandParams* vector1, const GCodeCommandParams* vector2);


#ifdef __cplusplus
}
#endif

#endif //__PRINTER_GCODE_FILE__
