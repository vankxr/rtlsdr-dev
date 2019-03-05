#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include "debug_macros.h"
#include "driver.h"
#include "fir.h"
#include "oscillator.h"
#include "iq.h"

#define CHANNEL_CARRIER     (101.4 * 1000000)   // 100.1 MHz - FM channel center frequency
#define CHANNEL_BANDWIDTH   (200 * 1000)        // 200 kHz - FM channel bandwidth
#define AUDIO_RATE          (44.1 * 1000)       // 44.1 kHz - Target audio sample rate
#define LO_FREQUENCY        (300 * 1000)        // 300 kHz - Sample with an offset to avoid DC spike
#define SAMPLE_RATE         (1.2 * 1000000)     // 1.2 Msps - RTL-SDR sample rate
#define SAMPLE_FREQUENCY    (CHANNEL_CARRIER + LO_FREQUENCY)

#define _BB_DOWNSAMPLE          (SAMPLE_RATE / CHANNEL_BANDWIDTH)
#define BB_DOWNSAMPLE           (uint32_t)_BB_DOWNSAMPLE
#define REAL_CHANNEL_BANDWIDTH  (SAMPLE_RATE / BB_DOWNSAMPLE)

#define _AUDIO_DOWNSAMPLE       (REAL_CHANNEL_BANDWIDTH / AUDIO_RATE)
#define AUDIO_DOWNSAMPLE        (uint32_t)_AUDIO_DOWNSAMPLE
#define REAL_AUDIO_RATE         (REAL_CHANNEL_BANDWIDTH / AUDIO_DOWNSAMPLE)

rtlsdr_t *pSDR;
oscillator_t *pChannelLO;
fir_filter_t *pPilot19kBandPass;
fir_filter_t *pPilot38kHighPass;
fir_filter_t *pPilot57kHighPass;
fir_filter_t *pStereoBandPass;
fir_filter_t *pRDSBandPass;
fir_filter_t *pAudioLowPass[2];
iq16_downsampler_t *pBasebandDownsampler;
int16_downsampler_t *pAudioDownsampler[2];
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

uint8_t stereo_audio_demod(int16_t sBaseband, int16_t sStereoPilot, int16_t *psLeft, int16_t *psRight)
{
    if(!psLeft)
        return 0;

    if(!psRight)
        return 0;

    // Mono audio (Left + Right)
    int16_t sMonoAudio = sBaseband;

    // Stereo audio (Left - Right)
    int16_t sStereoAudio = sBaseband;

    sStereoPilot >>= 6; // Suppress noise from the recovered carrier

    sStereoAudio = fir_filter(pStereoBandPass, sStereoAudio);

    sStereoAudio = ((int32_t)sStereoAudio * sStereoPilot) / INT8_MAX;

    // Downsample
    uint8_t ubMonoDownsample = int16_downsample(pAudioDownsampler[0], sMonoAudio, &sMonoAudio);
    uint8_t ubStereoDownsample = int16_downsample(pAudioDownsampler[1], sStereoAudio, &sStereoAudio);

    if(ubMonoDownsample != 2 || ubStereoDownsample != 2)
        return 0;

    // Filter
    sMonoAudio = fir_filter(pAudioLowPass[0], sMonoAudio);
    sStereoAudio = fir_filter(pAudioLowPass[1], sStereoAudio);

    // Mix
    *psLeft = sMonoAudio + sStereoAudio;
    *psRight = sMonoAudio - sStereoAudio;

    return 1;
}

void sample_handler(iq16_t xSample)
{
    iq16_t xLO = oscillator_get(pChannelLO, 0);

    xSample = IQ16_PRODUCT(xSample, xLO); // Bring the spectrum back to baseband
    xSample = IQ16_SCALAR_QUOTIENT(xSample, INT8_MAX); // Scale by INT8_MAX

    if(iq16_downsample(pBasebandDownsampler, xSample, &xSample) != 2) // Downsample to the channel bandwidth
        return;

    int16_t sBaseband = fm_demod(xSample); // Demodulate, signal is now real

    fwrite(&sBaseband, sizeof(int16_t), 1, pBasebandFile); // Write the baseband data

    // 19 kHz pilot and multiples
    int16_t sPilot19k = sBaseband;

    sPilot19k = fir_filter(pPilot19kBandPass, sPilot19k);

    //// Stereo Pilot
    int16_t sPilot38k = ((int32_t)sPilot19k * sPilot19k) / INT8_MAX;

    sPilot38k = fir_filter(pPilot38kHighPass, sPilot38k);

    //// RDS Pilot
    //int16_t sPilot57k = ((int32_t)sPilot38k * sPilot19k) / INT8_MAX;

    //sPilot57k = fir_filter(pPilot57kHighPass, sPilot57k);

    // Audio
    int16_t sLeft, sRight;

    if(stereo_audio_demod(sBaseband, sPilot38k, &sLeft, &sRight))
    {
        fwrite(&sLeft, sizeof(int16_t), 1, pAudioFile);
        fwrite(&sRight, sizeof(int16_t), 1, pAudioFile);
    }
}

void signal_handler(int iSignal)
{
    DBGPRINTLN_CTX("Got signal %d, stop sampling...", iSignal);

    driver_rtlsdr_sample_stop(pSDR);
}

int main(int argc, char **argv)
{
    stderr = freopen("/dev/null", "w", stderr);

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    DBGPRINTLN_CTX("Set SIGINT handler...");
    signal(SIGINT, signal_handler);

    DBGPRINTLN_CTX("Init audio components...");

    DBGPRINTLN_CTX("Setting audio LPF cutoff at %.2f kHz", (float)AUDIO_LPF_CUTOFF / 1000.f);

    pAudioLowPass[0] = fir_init(audio_lpf_coefs, AUDIO_LPF_TAP_NUM);

    if(!pAudioLowPass[0])
        return 1;

    pAudioLowPass[1] = fir_init(audio_lpf_coefs, AUDIO_LPF_TAP_NUM);

    if(!pAudioLowPass[1])
        return 1;

    DBGPRINTLN_CTX("Setting audio downsample factor to %u, audio rate %.2f kHz", AUDIO_DOWNSAMPLE, (float)REAL_AUDIO_RATE / 1000.f);

    pAudioDownsampler[0] = int16_downsampler_init(AUDIO_DOWNSAMPLE);

    if(!pAudioDownsampler[0])
        return 1;

    pAudioDownsampler[1] = int16_downsampler_init(AUDIO_DOWNSAMPLE);

    if(!pAudioDownsampler[1])
        return 1;

    DBGPRINTLN_CTX("Init baseband components...");

    DBGPRINTLN_CTX("Setting baseband downsample factor to %u, channel bandwidth %.2f kHz", BB_DOWNSAMPLE, (float)REAL_CHANNEL_BANDWIDTH / 1000.f);

    pBasebandDownsampler = iq16_downsampler_init(BB_DOWNSAMPLE);

    if(!pBasebandDownsampler)
        return 1;

    DBGPRINTLN_CTX("Setting 19 kHz pilot BPF low cutoff at %.2f kHz and high cutoff at %.2f kHz", (float)PILOT_19KHZ_BPF_LOW_CUTOFF / 1000.f,  (float)PILOT_19KHZ_BPF_HIGH_CUTOFF / 1000.f);

    pPilot19kBandPass = fir_init(pilot_19khz_bpf_coefs, PILOT_19KHZ_BPF_TAP_NUM);

    if(!pPilot19kBandPass)
        return 1;

    DBGPRINTLN_CTX("Setting 38 kHz pilot HPF high cutoff at %.2f kHz", (float)PILOT_38KHZ_HPF_CUTOFF / 1000.f);

    pPilot38kHighPass = fir_init(pilot_38khz_hpf_coefs, PILOT_38KHZ_HPF_TAP_NUM);

    if(!pPilot38kHighPass)
        return 1;

    DBGPRINTLN_CTX("Setting 57 kHz pilot HPF high cutoff at %.2f kHz", (float)PILOT_57KHZ_HPF_CUTOFF / 1000.f);

    pPilot57kHighPass = fir_init(pilot_57khz_hpf_coefs, PILOT_57KHZ_HPF_TAP_NUM);

    if(!pPilot57kHighPass)
        return 1;

    DBGPRINTLN_CTX("Setting stereo BPF low cutoff at %.2f kHz and high cutoff at %.2f kHz", (float)STEREO_BPF_LOW_CUTOFF / 1000.f,  (float)STEREO_BPF_HIGH_CUTOFF / 1000.f);

    pStereoBandPass = fir_init(stereo_bpf_coefs, STEREO_BPF_TAP_NUM);

    if(!pStereoBandPass)
        return 1;

    DBGPRINTLN_CTX("Setting RDS BPF low cutoff at %.2f kHz and high cutoff at %.2f kHz", (float)RDS_BPF_LOW_CUTOFF / 1000.f,  (float)RDS_BPF_HIGH_CUTOFF / 1000.f);

    pRDSBandPass = fir_init(rds_bpf_coefs, RDS_BPF_TAP_NUM);

    if(!pRDSBandPass)
        return 1;

    DBGPRINTLN_CTX("Init LO...");

    DBGPRINTLN_CTX("Setting LO frequency to %.2f kHz", (float)LO_FREQUENCY / 1000.f);

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

    DBGPRINTLN_CTX("Setting carrier to %.2f MHz", (float)CHANNEL_CARRIER / 1000000.f);
    DBGPRINTLN_CTX("Tuning SDR to %.2f MHz", (float)SAMPLE_FREQUENCY / 1000000.f);
    DBGPRINTLN_CTX("Setting SDR bandwidth to +/- %.2f kHz", (float)SAMPLE_RATE / 2.f / 1000.f);

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

    DBGPRINTLN_CTX("Deinit audio components...");
    fir_cleanup(pAudioLowPass[0]);
    fir_cleanup(pAudioLowPass[1]);
    int16_downsampler_cleanup(pAudioDownsampler[0]);
    int16_downsampler_cleanup(pAudioDownsampler[1]);

    DBGPRINTLN_CTX("Deinit baseband components...");
    fir_cleanup(pPilot19kBandPass);
    fir_cleanup(pPilot38kHighPass);
    fir_cleanup(pPilot57kHighPass);
    fir_cleanup(pStereoBandPass);
    fir_cleanup(pRDSBandPass);
    iq16_downsampler_cleanup(pBasebandDownsampler);

    DBGPRINTLN_CTX("Deinit LO...");
    oscillator_cleanup(pChannelLO);


    return 0;
}