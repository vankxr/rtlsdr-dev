#include "oscillator.h"

oscillator_t *oscillator_init(uint32_t ulSampleRate, uint32_t ulFreqneucy)
{
    oscillator_t *pOscillator = (oscillator_t *)malloc(sizeof(oscillator_t));

    if(!pOscillator)
        return NULL;

    memset(pOscillator, 0, sizeof(oscillator_t));

    pOscillator->ulSteps = ulSampleRate / ulFreqneucy;
    pOscillator->ulCurrentPhase = 0;

    pOscillator->pData = (iq16_t *)malloc(sizeof(iq16_t) * pOscillator->ulSteps);

    if(!pOscillator->pData)
    {
        free(pOscillator);

        return NULL;
    }

    for (uint32_t t = 0; t < pOscillator->ulSteps; t++)
    {
        pOscillator->pData[t].i = INT8_MAX * cos(2 * M_PI * (float)t / pOscillator->ulSteps);
        pOscillator->pData[t].q = INT8_MAX * sin(2 * M_PI * (float)t / pOscillator->ulSteps);
    }

    return pOscillator;
}
void oscillator_cleanup(oscillator_t *pOscillator)
{
    if(!pOscillator)
        return;

    if(pOscillator->pData)
        free(pOscillator->pData);

    free(pOscillator);
}
iq16_t oscillator_get(oscillator_t *pOscillator, uint32_t ulPhaseOffset)
{
    iq16_t xResult = {0, 0};

    if(!pOscillator)
        return xResult;

    if(!pOscillator->pData)
        return xResult;

    xResult = pOscillator->pData[pOscillator->ulCurrentPhase + ulPhaseOffset];

    pOscillator->ulCurrentPhase = (pOscillator->ulCurrentPhase + 1) % (abs(pOscillator->ulSteps));

    return xResult;
}