#ifndef __OSCILLATOR_H__
#define __OSCILLATOR_H__

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "iq.h"

typedef struct oscillator_t
{
    uint32_t ulSteps;
    uint32_t ulCurrentPhase;
    iq16_t *pData;
} oscillator_t;

oscillator_t *oscillator_init(uint32_t ulSampleRate, int32_t lFreqneucy);
void oscillator_cleanup(oscillator_t *pOscillator);
iq16_t oscillator_get(oscillator_t *pOscillator, int32_t lPhaseOffset);

#endif