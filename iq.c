#include "iq.h"

uint8_t iq16_downsample(iq_downsampler_t *pDownsampler, iq16_t xIn, iq16_t *pxOut)
{
    if(!pDownsampler)
        return 0;

    pDownsampler->i += xIn.i;
    pDownsampler->q += xIn.q;
    pDownsampler->ulCount++;

    if(pDownsampler->ulCount == pDownsampler->ulDownSample)
    {
        if(pxOut)
        {
            pxOut->i = (float)pDownsampler->i / pDownsampler->ulDownSample;
            pxOut->q = (float)pDownsampler->q / pDownsampler->ulDownSample;
        }

        pDownsampler->i = 0;
        pDownsampler->q = 0;
        pDownsampler->ulCount = 0;

        return 2;
    }

    return 1;
}