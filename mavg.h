#ifndef __MAVG_H__
#define __MAVG_H__

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
    uint32_t ulSize;
    int16_t *psData;
    uint16_t usIndex;
    int32_t lCount;
    int32_t lCountHold;
    uint8_t ubHold;
} mavg_filter_t;

mavg_filter_t *mavg_init(uint32_t ulSize);
void mavg_cleanup(mavg_filter_t *pFilter);
int32_t mavg_count(mavg_filter_t *pFilter, int16_t sSample);
int16_t mavg_high_pass(mavg_filter_t *pFilter, int16_t sSample);
int16_t mavg_low_pass(mavg_filter_t *pFilter, int16_t sSample);
int16_t mavg_band_pass(mavg_filter_t *pHighFilter, mavg_filter_t *pLowFilter, int16_t sSample);

#endif