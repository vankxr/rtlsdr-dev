#include "downsampler.h"

int16_downsampler_t *int16_downsampler_init(uint32_t ulDownsample)
{
    int16_downsampler_t *pDownsampler = (int16_downsampler_t *)malloc(sizeof(int16_downsampler_t));

    if(!pDownsampler)
        return NULL;

    pDownsampler->llAccumulator = 0;
    pDownsampler->ulCount = 0;
    pDownsampler->ulDownsample = ulDownsample;

    return pDownsampler;
}
void int16_downsampler_cleanup(int16_downsampler_t *pDownsampler)
{
    if(!pDownsampler)
        return;

    free(pDownsampler);
}
uint8_t int16_downsample(int16_downsampler_t *pDownsampler, int16_t sIn, int16_t *psOut)
{
    if(!pDownsampler)
        return 0;

    pDownsampler->llAccumulator += sIn;
    pDownsampler->ulCount++;

    if(pDownsampler->ulCount == pDownsampler->ulDownsample)
    {
        if(psOut)
            *psOut = (float)pDownsampler->llAccumulator / pDownsampler->ulDownsample;

        pDownsampler->llAccumulator = 0;
        pDownsampler->ulCount = 0;

        return 2;
    }

    return 1;
}

iq16_downsampler_t *iq16_downsampler_init(uint32_t ulDownsample)
{
    iq16_downsampler_t *pDownsampler = (iq16_downsampler_t *)malloc(sizeof(iq16_downsampler_t));

    if(!pDownsampler)
        return NULL;

    pDownsampler->llIAccumulator = 0;
    pDownsampler->llQAccumulator = 0;
    pDownsampler->ulCount = 0;
    pDownsampler->ulDownsample = ulDownsample;

    return pDownsampler;
}
void iq16_downsampler_cleanup(iq16_downsampler_t *pDownsampler)
{
    if(!pDownsampler)
        return;

    free(pDownsampler);
}
uint8_t iq16_downsample(iq16_downsampler_t *pDownsampler, iq16_t xIn, iq16_t *pxOut)
{
    if(!pDownsampler)
        return 0;

    pDownsampler->llIAccumulator += xIn.i;
    pDownsampler->llQAccumulator += xIn.q;
    pDownsampler->ulCount++;

    if(pDownsampler->ulCount == pDownsampler->ulDownsample)
    {
        if(pxOut)
        {
            pxOut->i = (float)pDownsampler->llIAccumulator / pDownsampler->ulDownsample;
            pxOut->q = (float)pDownsampler->llQAccumulator / pDownsampler->ulDownsample;
        }

        pDownsampler->llIAccumulator = 0;
        pDownsampler->llQAccumulator = 0;
        pDownsampler->ulCount = 0;

        return 2;
    }

    return 1;
}