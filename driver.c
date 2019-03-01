#include "driver.h"

static void rtlsdr_callback(uint8_t *pubData, uint32_t ulDataSize, void *pContext)
{
    if(!pContext)
        return;

    rtlsdr_t *pSDR = pContext;

    for (uint32_t i = 0; i < ulDataSize; i += 2)
    {
        iq16_t xSample;

        xSample.i = pubData[i + 0] - 128;
        xSample.q = pubData[i + 1] - 128;

        if(iq16_downsample(&pSDR->xDownsampler, xSample, &xSample) == 2)
            pSDR->pfSampleHandler(xSample);
    }
}
#if defined (USE_THREAD)
static void *driver_thread_fn(void *pContext)
{
    if(!pContext)
        return NULL;

    rtlsdr_t *pSDR = pContext;

    rtlsdr_read_async(pSDR->pDevice, rtlsdr_callback, pSDR, 0, DRIVER_BUFFER_SIZE);

    return NULL;
}
#endif


rtlsdr_t *driver_rtlsdr_init(int32_t lFrequency, int32_t lSampleRate, int32_t lGain, int32_t lPPM, rtlsdr_sample_handler_t pfSampleHandler)
{
    if(!pfSampleHandler)
        return NULL;

    if(!lFrequency)
        return NULL;

    if(!lSampleRate)
        return NULL;

    rtlsdr_t *pSDR = (rtlsdr_t *)malloc(sizeof(rtlsdr_t));

    if(!pSDR)
        return NULL;

    memset(pSDR, 0, sizeof(rtlsdr_t));

    pSDR->pfSampleHandler = pfSampleHandler;

    pSDR->xDownsampler.ulDownSample = 1;
    pSDR->xDownsampler.i = 0;
    pSDR->xDownsampler.q = 0;
    pSDR->xDownsampler.ulCount = 0;

    while (!IS_VALID_SAMPLERATE(lSampleRate * pSDR->xDownsampler.ulDownSample))
    {
        pSDR->xDownsampler.ulDownSample++;

        if (lSampleRate * pSDR->xDownsampler.ulDownSample > 2400000)
        {
            free(pSDR);

            return NULL;
        }
    }

    if (rtlsdr_open(&pSDR->pDevice, 0) < 0)
    {
        free(pSDR);

        return NULL;
    }

    if (rtlsdr_set_center_freq(pSDR->pDevice, lFrequency) < 0) // Set freq before sample rate to avoid "PLL NOT LOCKED"
    {
        free(pSDR);

        return NULL;
    }

    if (rtlsdr_set_sample_rate(pSDR->pDevice, lSampleRate * pSDR->xDownsampler.ulDownSample) < 0)
    {
        free(pSDR);

        return NULL;
    }

    if (rtlsdr_set_agc_mode(pSDR->pDevice, 0) < 0)
    {
        free(pSDR);

        return NULL;
    }

    if (rtlsdr_set_tuner_gain_mode(pSDR->pDevice, lGain != -1) < 0)
    {
        free(pSDR);

        return NULL;
    }

    if (lGain != -1 && rtlsdr_set_tuner_gain(pSDR->pDevice, lGain) < 0)
    {
        free(pSDR);

        return NULL;
    }

    if (lPPM && rtlsdr_set_freq_correction(pSDR->pDevice, lPPM) < 0)
    {
        free(pSDR);

        return NULL;
    }

    if (rtlsdr_reset_buffer(pSDR->pDevice) < 0)
    {
        free(pSDR);

        return NULL;
    }

#if defined (USE_THREAD)
    // Create thread
#endif

    return pSDR;
}
void driver_rtlsdr_cancel(rtlsdr_t *pSDR)
{
    if(!pSDR)
        return;

    rtlsdr_cancel_async(pSDR->pDevice);
}
void driver_rtlsdr_cleanup(rtlsdr_t *pSDR)
{
    if(!pSDR)
        return;

    rtlsdr_close(pSDR->pDevice);

    free(pSDR);
}

#if !defined(USE_THREAD)
void driver_rtlsdr_sample(rtlsdr_t *pSDR)
{
    if(!pSDR)
        return;

    rtlsdr_read_async(pSDR->pDevice, rtlsdr_callback, pSDR, 0, DRIVER_BUFFER_SIZE);
}
#endif