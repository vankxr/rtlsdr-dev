#include "mavg.h"

mavg_filter_t *mavg_init(uint32_t ulSize)
{
    mavg_filter_t *pFilter = (mavg_filter_t *)malloc(sizeof(mavg_filter_t));

    if(!pFilter)
        return NULL;

    memset(pFilter, 0, sizeof(mavg_filter_t));

    pFilter->psData = (int16_t *)malloc(sizeof(int16_t) * ulSize);

    if(!pFilter->psData)
    {
        free(pFilter);

        return NULL;
    }

    memset(pFilter->psData, 0, sizeof(int16_t) * ulSize);

    pFilter->ulSize = ulSize;
    pFilter->usIndex = 0;
    pFilter->lCount = 0;
    pFilter->lCountHold = 0;
    pFilter->ubHold = 0;

    return pFilter;
}
void mavg_cleanup(mavg_filter_t *pFilter)
{
    if(!pFilter)
        return;

    if(pFilter->psData)
        free(pFilter->psData);

    free(pFilter);
}
int32_t mavg_count(mavg_filter_t *pFilter, int16_t sSample)
{
    if(!pFilter)
        return 0;

    pFilter->usIndex = (pFilter->usIndex + 1) % pFilter->ulSize;
    pFilter->lCount += sSample - pFilter->psData[pFilter->usIndex];
    pFilter->psData[pFilter->usIndex] = sSample;

    return pFilter->lCount;
}

int16_t mavg_high_pass(mavg_filter_t *pFilter, int16_t sSample)
{
    if(!pFilter)
        return 0;

    if (!pFilter->ubHold)
        pFilter->lCountHold = mavg_count(pFilter, sSample);

    return sSample - (pFilter->lCountHold / pFilter->ulSize);
}
int16_t mavg_low_pass(mavg_filter_t *pFilter, int16_t sSample)
{
    if(!pFilter)
        return 0;

    return mavg_count(pFilter, sSample) / pFilter->ulSize;
}
int16_t mavg_band_pass(mavg_filter_t *pHighFilter, mavg_filter_t *pLowFilter, int16_t sSample)
{
    if(!pHighFilter)
        return 0;

    if(!pLowFilter)
        return 0;

    return mavg_high_pass(pHighFilter, mavg_low_pass(pLowFilter, sSample));
}
