#include "printer_file_manager.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>


static uint32_t compareTimeWithSpeedLimit(uint32_t fetch_speed, int32_t signed_segment, uint32_t time, uint16_t resolution)
{
    uint32_t segment_length = signed_segment;
    // distance in time is always positive
    if (signed_segment < 0)
    {
        segment_length = (uint32_t)(-signed_segment);
    }

    // check if we reached speed limit: amount of steps required to reach destination lower than amount of requested steps 
    if (fetch_speed && (float)(MAIN_TIMER_FREQUENCY * SECONDS_IN_MINUTE) / (float)(resolution * fetch_speed) > 1)
    {
        segment_length = segment_length * ((float)(MAIN_TIMER_FREQUENCY * SECONDS_IN_MINUTE) / (float)(resolution * fetch_speed));
    }
    // return the longest distance
    return segment_length > time ? segment_length : time;
}

uint32_t CalculateTime(const GCodeAxisConfig* axis_cfg, const GCodeCommandParams* segment)
{
    // calculate max time required to complete the current command
    uint32_t time = compareTimeWithSpeedLimit(segment->fetch_speed, 
                                                (int32_t)(sqrt((double)segment->x * segment->x + (double)segment->y * segment->y) + 0.5), 
                                                0U, axis_cfg->x_steps_per_mm);
    time = compareTimeWithSpeedLimit(segment->fetch_speed, segment->z, time, axis_cfg->z_steps_per_mm);
    time = compareTimeWithSpeedLimit(segment->fetch_speed, segment->e, time, axis_cfg->e_steps_per_mm);
    return time;
}

uint32_t CalculateSegmentTime(const GCodeAxisConfig* axis_cfg, 
                                     const GCodeCommandParams* current_point, 
                                     const GCodeCommandParams* previous_point)
{
    if (current_point->fetch_speed <= 0)
    {
        return 0;
    }

    // calulate the current segment length
    GCodeCommandParams segment = *current_point;
    segment.x -= previous_point->x;
    segment.y -= previous_point->y;
    segment.z -= previous_point->z;
    segment.e -= previous_point->e;

    // basic fetch speed is calculated as velocity of the head, without velocity of the table, it is calculated independently.
    return CalculateTime(axis_cfg, &segment);
}

/// <summary>
/// Dot product of two vectors
/// </summary>
double Dot(const GCodeCommandParams* vector1, const GCodeCommandParams* vector2)
{
    return (double)vector1->x * vector2->x + (double)vector1->y * vector2->y + (double)vector1->z * vector2->z;
}