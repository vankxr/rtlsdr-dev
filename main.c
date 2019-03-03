#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include "debug_macros.h"
#include "driver.h"
#include "mavg.h"
#include "fir.h"
#include "oscillator.h"
#include "iq.h"

#define AUDIO_RATE          (44.1 * 1000) // 44.1 kHz
#define CHANNEL_CARRIER     (100.1 * 1000000) // 100.1 MHz
#define CHANNEL_BANDWIDTH   (200 * 1000) // 200 kHz
#define LO_FREQUENCY        (300 * 1000) // 300 kHz (to avoid DC spike)
#define SAMPLE_RATE         (1.2 * 1000000) // 1.2 Msps (RTL-SDR sample rate)
#define SAMPLE_FREQUENCY    (CHANNEL_CARRIER + LO_FREQUENCY)

#define BB_DOWNSAMPLE           (uint32_t)(SAMPLE_RATE / CHANNEL_BANDWIDTH)
#define REAL_CHANNEL_BANDWIDTH  (SAMPLE_RATE / BB_DOWNSAMPLE)

#define AUDIO_DOWNSAMPLE        (uint32_t)(REAL_CHANNEL_BANDWIDTH / AUDIO_RATE)
#define REAL_AUDIO_RATE         (REAL_CHANNEL_BANDWIDTH / AUDIO_DOWNSAMPLE)

rtlsdr_t *pSDR;
oscillator_t *pChannelLO;
mavg_filter_t *pAudioHighPass;
fir_filter_t *pAudioLowPass;
fir_filter_t *pBasebandLowPass[2];
iq_downsampler_t xBasebandDownsampler;
iq_downsampler_t xAudioDownsampler;
FILE *pBasebandFile;
FILE *pAudioFile;

uint16_t abs16(int16_t value)
{
    uint16_t sign = value >> 15;     // make a mask of the sign bit
    value ^= sign;                   // toggle the bits if value is negative
    value += sign & 1;               // add one if value was negative
    return value;
}

int16_t atan2_int16(int16_t y, int16_t x)
{
    uint16_t absx = abs16(x);
    uint16_t absy = abs16(y);

    if(absx > INT16_MAX)
        absx = INT16_MAX;

    if(absy > INT16_MAX)
        absy = INT16_MAX;

    int32_t denominator = absy + absx;

    if (denominator == 0)
        return 0; // avoid DBZ

    int32_t numerator = (int32_t)(TAU / 8) * (int32_t)(absy - absx);

    int16_t theta = ((numerator << 3) / denominator) >> 3;

    if (y >= 0) // Note: Cartesian plane quadrants
    {
        if (x >= 0)
            return (TAU * 1 / 8) + theta; // quadrant I    Theta counts 'towards the y axis',
        else
            return (TAU * 3 / 8) - theta; // quadrant II   So, negate it in quadrants II and IV
    }
    else
    {
        if (x < 0)
            return (TAU * -3 / 8) + theta; // quadrant III. -3/8 = 5/8
        else
            return (TAU * -1 / 8) - theta; // quadrant IV.  -1/8 = 7/8
    }
}

iq16_t fm_demod(iq16_t xSample)
{
    static iq16_t xPrevSample = {0, 0};

    iq16_t xDiff = IQ16_PRODUCT(xSample, IQ16_CONJUGATE(xPrevSample));

    xPrevSample = xSample;

    return INT16_TO_IQ16(atan2_int16(xDiff.q, xDiff.i));
}

void sample_handler(iq16_t xSample)
{
    iq16_t xLO = oscillator_get(pChannelLO, 0);

    xSample = IQ16_SCALAR_QUOTIENT(IQ16_PRODUCT(xSample, xLO), INT8_MAX); // Spectrum centered at fCarrier now

    xSample.i = fir_filter(pBasebandLowPass[0], xSample.i);
    xSample.q = fir_filter(pBasebandLowPass[1], xSample.q);

    if(iq16_downsample(&xBasebandDownsampler, xSample, &xSample) != 2)
        return;

    iq16_t xSignal = fm_demod(xSample);

    fwrite(&xSignal.i, sizeof(int16_t), 1, pBasebandFile);

    //xSignal.i = mavg_high_pass(pAudioHighPass, xSignal.i); // DC remove
    xSignal.i = fir_filter(pAudioLowPass, xSignal.i);

    if(iq16_downsample(&xAudioDownsampler, xSignal, &xSignal) != 2)
        return;

    fwrite(&xSignal.i, sizeof(int16_t), 1, pAudioFile);
}

void signal_handler(int iSignal)
{
    DBGPRINTLN_CTX("Got signal %d, cancelling...", iSignal);

    driver_rtlsdr_cancel(pSDR);
}

int main (int argc, char **argv)
{
    DBGPRINTLN_CTX("Set signal handler...");
    signal(SIGINT, signal_handler);

    DBGPRINTLN_CTX("Init audio filters...");

    float fLowCutoff = 10;
    uint32_t ulHighpassSize = (0.443f * REAL_CHANNEL_BANDWIDTH) / fLowCutoff;

    ulHighpassSize = (ulHighpassSize < 1) ? 1 : ulHighpassSize;

    DBGPRINTLN_CTX("Setting audio filters at %.0f Hz < x < %d Hz", fLowCutoff, AUDIO_LPF_CUTOFF);

    pAudioHighPass = mavg_init(ulHighpassSize);

    if(!pAudioHighPass)
        return 1;

    pAudioLowPass = fir_init(audio_lpf_coefs, AUDIO_LPF_TAP_NUM);

    if(!pAudioLowPass)
        return 1;

    DBGPRINTLN_CTX("Setting audio downsample factor to %u, audio rate %.2f kHz", AUDIO_DOWNSAMPLE, (float)REAL_AUDIO_RATE / 1000.f);

    xAudioDownsampler.i = 0;
    xAudioDownsampler.q = 0;
    xAudioDownsampler.ulCount = 0;
    xAudioDownsampler.ulDownSample = AUDIO_DOWNSAMPLE;

    DBGPRINTLN_CTX("Init baseband components...");

    DBGPRINTLN_CTX("Setting baseband LPF at %d Hz < x", BB_LPF_CUTOFF);

    pBasebandLowPass[0] = fir_init(bb_lpf_coefs, BB_LPF_TAP_NUM);

    if(!pBasebandLowPass[0])
        return 1;

    pBasebandLowPass[1] = fir_init(bb_lpf_coefs, BB_LPF_TAP_NUM);

    if(!pBasebandLowPass[1])
        return 1;

    DBGPRINTLN_CTX("Setting baseband downsample factor to %u, channel bandwidth %.2f kHz", BB_DOWNSAMPLE, (float)REAL_CHANNEL_BANDWIDTH / 1000.f);

    xBasebandDownsampler.i = 0;
    xBasebandDownsampler.q = 0;
    xBasebandDownsampler.ulCount = 0;
    xBasebandDownsampler.ulDownSample = BB_DOWNSAMPLE;

    DBGPRINTLN_CTX("Init LO...");

    pChannelLO = oscillator_init(SAMPLE_RATE, LO_FREQUENCY);

    if(!pChannelLO)
        return 1;

    DBGPRINTLN_CTX("Init audio file...");
    pAudioFile = fopen("./audio_out.raw", "w");
    pBasebandFile = fopen("./bb.raw", "w");

    if(!pAudioFile)
        return 1;

    DBGPRINTLN_CTX("Init RTLSDR...");

    pSDR = driver_rtlsdr_init(SAMPLE_FREQUENCY, SAMPLE_RATE, -99, 0, sample_handler);

    if(!pSDR)
        return 1;

    DBGPRINTLN_CTX("Sample...");
    driver_rtlsdr_sample(pSDR);

    DBGPRINTLN_CTX("Deinit RTLSDR...");
    driver_rtlsdr_cleanup(pSDR);

    DBGPRINTLN_CTX("Deinit audio file...");
    fclose(pAudioFile);
    fclose(pBasebandFile);

    DBGPRINTLN_CTX("Deinit audio filters...");
    mavg_cleanup(pAudioHighPass);
    fir_cleanup(pAudioLowPass);

    DBGPRINTLN_CTX("Deinit baseband filters...");
    fir_cleanup(pBasebandLowPass[0]);
    fir_cleanup(pBasebandLowPass[1]);

    DBGPRINTLN_CTX("Deinit LO...");
    oscillator_cleanup(pChannelLO);

    return 0;
}