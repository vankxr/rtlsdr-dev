#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <rtl-sdr.h>
#include "iq.h"

// Valid sample rates from the driver lib:
// 225001 - 300000 Hz
// 900001 - 3200000 Hz
// sample loss is to be expected for rates > 2400000
#define IS_VALID_SAMPLERATE(x) ((x >= 225001 && x <= 300000) || (x >= 900001 && x <= 3200000))
#define DRIVER_BUFFER_SIZE (256 * 1024)
//#define USE_THREAD

typedef void (* rtlsdr_sample_handler_t)(iq16_t);

typedef struct rtlsdr_t
{
	rtlsdr_dev_t *pDevice;
	iq_downsampler_t xDownsampler;
	rtlsdr_sample_handler_t pfSampleHandler;
} rtlsdr_t;

rtlsdr_t *driver_rtlsdr_init(int32_t lFrequency, int32_t lSampleRate, int32_t lGain, int32_t lPPM, rtlsdr_sample_handler_t pfSampleHandler);
void driver_rtlsdr_cancel(rtlsdr_t *pSDR);
void driver_rtlsdr_cleanup(rtlsdr_t *pSDR);

#if !defined(USE_THREAD)
void driver_rtlsdr_sample(rtlsdr_t *pSDR);
#endif

#endif