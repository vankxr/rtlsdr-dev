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

#define CHANNEL_CARRIER     (102.0 * 1000000) // 100.1 MHz - FM channel center frequency
#define CHANNEL_BANDWIDTH   (200 * 1000) // 200 kHz - FM channel bandwidth
#define AUDIO_RATE          (44.1 * 1000) // 44.1 kHz - Target audio sample rate
#define LO_FREQUENCY        (300 * 1000) // 300 kHz - Sample with an offset to avoid DC spike
#define SAMPLE_RATE         (1.2 * 1000000) // 1.2 Msps - RTL-SDR sample rate
#define SAMPLE_FREQUENCY    (CHANNEL_CARRIER + LO_FREQUENCY)

#define _BB_DOWNSAMPLE          (SAMPLE_RATE / CHANNEL_BANDWIDTH)
#define BB_DOWNSAMPLE           (uint32_t)_BB_DOWNSAMPLE
#define REAL_CHANNEL_BANDWIDTH  (SAMPLE_RATE / BB_DOWNSAMPLE)

#define _AUDIO_DOWNSAMPLE       (REAL_CHANNEL_BANDWIDTH / AUDIO_RATE)
#define AUDIO_DOWNSAMPLE        (uint32_t)_AUDIO_DOWNSAMPLE
#define REAL_AUDIO_RATE         (REAL_CHANNEL_BANDWIDTH / AUDIO_DOWNSAMPLE)

rtlsdr_t *pSDR;
oscillator_t *pChannelLO;
mavg_filter_t *pAudioHighPass;
fir_filter_t *pAudioLowPass;
fir_filter_t *pBasebandLowPass[2];
iq16_downsampler_t *pBasebandDownsampler;
int16_downsampler_t *pAudioDownsampler;
FILE *pBasebandFile;
FILE *pAudioFile;

uint16_t abs16(int16_t sValue)
{
    uint16_t usSign = sValue >> 15;     // make a mask of the sign bit

    sValue ^= usSign;                   // toggle the bits if value is negative
    sValue += usSign & 1;               // add one if value was negative

    return sValue;
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

int16_t fm_demod(iq16_t xSample)
{
    static iq16_t xPrevSample = {0, 0};

    iq16_t xDiff = IQ16_PRODUCT(xSample, IQ16_CONJUGATE(xPrevSample));

    xPrevSample = xSample;

    return atan2_int16(xDiff.q, xDiff.i);
}

void sample_handler(iq16_t xSample)
{
    iq16_t xLO = oscillator_get(pChannelLO, 0);

    xSample = IQ16_PRODUCT(xSample, xLO); // Bring the spectrum back to baseband
    xSample = IQ16_SCALAR_QUOTIENT(xSample, INT8_MAX); // Scale by INT8_MAX

    xSample.i = fir_filter(pBasebandLowPass[0], xSample.i); // Low pass filter to get only the centered channel
    xSample.q = fir_filter(pBasebandLowPass[1], xSample.q);

    if(iq16_downsample(pBasebandDownsampler, xSample, &xSample) != 2) // Downsample to the channel bandwidth
        return;

    int16_t sBaseband = fm_demod(xSample); // Demodulate, signal is now real

    sBaseband /= INT8_MAX; // Scale by INT8_MAX since demod returns angles in INT16_MAX range

    fwrite(&sBaseband, sizeof(int16_t), 1, pBasebandFile); // Write the baseband data

    // Mono audio
    int16_t sAudio = sBaseband;

    sAudio = mavg_high_pass(pAudioHighPass, sAudio); // DC remove
    sAudio = fir_filter(pAudioLowPass, sAudio);

    if(int16_downsample(pAudioDownsampler, sAudio, &sAudio) != 2) // Downsample audio to target sample rate
        return;

    fwrite(&sAudio, sizeof(int16_t), 1, pAudioFile);
}

void signal_handler(int iSignal)
{
    DBGPRINTLN_CTX("Got signal %d, stop sampling...", iSignal);

    driver_rtlsdr_sample_stop(pSDR);
}

int main(int argc, char **argv)
{
    DBGPRINTLN_CTX("Set signal handler...");
    signal(SIGINT, signal_handler);

    DBGPRINTLN_CTX("Init audio components...");

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

    pAudioDownsampler = int16_downsampler_init(AUDIO_DOWNSAMPLE);

    if(!pAudioDownsampler)
        return 1;

    DBGPRINTLN_CTX("Init baseband components...");

    DBGPRINTLN_CTX("Setting baseband LPF at %d Hz < x", BB_LPF_CUTOFF);

    pBasebandLowPass[0] = fir_init(bb_lpf_coefs, BB_LPF_TAP_NUM);

    if(!pBasebandLowPass[0])
        return 1;

    pBasebandLowPass[1] = fir_init(bb_lpf_coefs, BB_LPF_TAP_NUM);

    if(!pBasebandLowPass[1])
        return 1;

    DBGPRINTLN_CTX("Setting baseband downsample factor to %u, channel bandwidth %.2f kHz", BB_DOWNSAMPLE, (float)REAL_CHANNEL_BANDWIDTH / 1000.f);

    pBasebandDownsampler = iq16_downsampler_init(BB_DOWNSAMPLE);

    if(!pBasebandDownsampler)
        return 1;

    DBGPRINTLN_CTX("Init LO...");

    pChannelLO = oscillator_init(SAMPLE_RATE, LO_FREQUENCY);

    if(!pChannelLO)
        return 1;

    DBGPRINTLN_CTX("Init output files...");
    pAudioFile = fopen("./audio_out.raw", "w");

    if(!pAudioFile)
        return 1;

    pBasebandFile = fopen("./bb.raw", "w");

    if(!pBasebandFile)
        return 1;

    DBGPRINTLN_CTX("Init RTLSDR...");

    pSDR = driver_rtlsdr_init(SAMPLE_FREQUENCY, SAMPLE_RATE, -1, 0, sample_handler);

    if(!pSDR)
        return 1;

    DBGPRINTLN_CTX("Sample...");
    driver_rtlsdr_sample_start(pSDR);

    DBGPRINTLN_CTX("Deinit RTLSDR...");
    driver_rtlsdr_cleanup(pSDR);

    DBGPRINTLN_CTX("Deinit output files...");
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