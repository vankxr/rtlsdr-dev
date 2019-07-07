#ifndef __DOWNSAMPLER_H__
#define __DOWNSAMPLER_H__

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "iq.h"

typedef struct int16_downsampler_t
{
	int64_t llAccumulator;
    uint32_t ulCount;
	uint32_t ulDownsample;
} int16_downsampler_t;

typedef struct iq16_downsampler_t
{
	int64_t llIAccumulator;
	int64_t llQAccumulator;
    uint32_t ulCount;
	uint32_t ulDownsample;
} iq16_downsampler_t;

int16_downsampler_t *int16_downsampler_init(uint32_t ulDownsample);
void int16_downsampler_cleanup(int16_downsampler_t *pDownsampler);
uint8_t int16_downsample(int16_downsampler_t *pDownsampler, int16_t sIn, int16_t *psOut);

iq16_downsampler_t *iq16_downsampler_init(uint32_t ulDownsample);
void iq16_downsampler_cleanup(iq16_downsampler_t *pDownsampler);
uint8_t iq16_downsample(iq16_downsampler_t *pDownsampler, iq16_t xIn, iq16_t *pxOut);

#endif