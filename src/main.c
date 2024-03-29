#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <lame/lame.h>
#include "debug_macros.h"
#include "driver.h"
#include "fir.h"
#include "oscillator.h"
#include "iq.h"
#include "kiss_fft.h"

#define AUDIO_RATE          (47.5 * 1000)       // 47.5 kHz - Target audio sample rate (should be between 44 kHz and 48 kHz, 47,5 kHz is a multiple of the used channel bandwidth)
#define RDS_BITRATE         (1.1875 * 1000)     // 1.1875 kbps - RDS bit rate
#define CHANNEL_CARRIER     (107.7 * 1000000)   // 107.7 MHz - FM channel center frequency (carrier) [101.3 - Record Leiria, 107.7 - RFM Leiria, 105.4 - Cascais Rock FM, 104.3 - M80 Lisboa]
#define CHANNEL_BANDWIDTH   (190 * 1000)        // 190 kHz - FM baseband bandwidth (should be 200 kHz but 190 kHz is more convenient and we do not lose any important information)
#define LO_FREQUENCY        (380 * 1000)        // 380 kHz - LO frequency to shift the signal back to baseband after sampling (to avoid DC spike)
#define SAMPLE_RATE         (1.9 * 1000000)     // 1.9 Msps - RTL-SDR sample rate (multiple of the demodulated FM baseband subcarriers)
#define SAMPLE_FREQUENCY    (CHANNEL_CARRIER + LO_FREQUENCY) // Sample with an offset to avoid the DC spike generated by Zero-IF and 1/f noise

#define SQUELCH_FFT_SIZE    4096
#define SQUELCH_LEVEL       -60     // -60 dBFS

#define MP3_BUFFER_SIZE     8192

// Baseband
#define _BB_DOWNSAMPLE              (SAMPLE_RATE / CHANNEL_BANDWIDTH)
#define BB_DOWNSAMPLE               (uint32_t)_BB_DOWNSAMPLE
#define REAL_CHANNEL_SAMPLE_RATE    (SAMPLE_RATE / BB_DOWNSAMPLE)

// Audio
#define _AUDIO_DOWNSAMPLE           (REAL_CHANNEL_SAMPLE_RATE / AUDIO_RATE)
#define AUDIO_DOWNSAMPLE            (uint32_t)_AUDIO_DOWNSAMPLE
#define REAL_AUDIO_SAMPLE_RATE      (REAL_CHANNEL_SAMPLE_RATE / AUDIO_DOWNSAMPLE)

// RDS
#define _RDS_BB_DOWNSAMPLE          (REAL_CHANNEL_SAMPLE_RATE / (10 * RDS_BITRATE))
#define RDS_BB_DOWNSAMPLE           (uint32_t)_RDS_BB_DOWNSAMPLE
#define REAL_RDS_BB_SAMPLE_RATE     (REAL_CHANNEL_SAMPLE_RATE / RDS_BB_DOWNSAMPLE)

#define _RDS_BIT_DOWNSAMPLE         (REAL_RDS_BB_SAMPLE_RATE / RDS_BITRATE)
#define RDS_BIT_DOWNSAMPLE          (uint32_t)_RDS_BIT_DOWNSAMPLE
#define REAL_RDS_BIT_SAMPLE_RATE    (REAL_RDS_BB_SAMPLE_RATE / RDS_BIT_DOWNSAMPLE)

lame_t xMP3Encoder = NULL;
uint8_t *pMP3Buffer = NULL;
rtlsdr_t *pSDR = NULL;
kiss_fft_cfg xSquelchFFT = NULL;
iq16_t *pSquelchFFTInput = NULL;
iq16_t *pSquelchFFTOutput = NULL;
oscillator_t *pChannelLO = NULL;
oscillator_t *pRDSLO = NULL;
fir_filter_t *pBasebandLowPass[2] = {NULL, NULL};
fir_filter_t *pPilot19kBandPass = NULL;
fir_filter_t *pPilot38kHighPass = NULL;
fir_filter_t *pPilot57kHighPass = NULL;
fir_filter_t *pStereoBandPass = NULL;
fir_filter_t *pRDSBandPass = NULL;
fir_filter_t *pRDSLowPass = NULL;
fir_filter_t *pAudioLowPass[2] = {NULL, NULL};
iq16_downsampler_t *pBasebandDownsampler = NULL;
int16_downsampler_t *pAudioDownsampler[2] = {NULL, NULL};
int16_downsampler_t *pRDSBBDownsampler = NULL;
int16_downsampler_t *pRDSBitDownsampler = NULL;
FILE *pBasebandFile = NULL;
FILE *pAudioFile = NULL;
FILE *pRDSFile = NULL;

uint8_t util_fft(iq16_t xSample, uint32_t ulFFTSize, uint32_t ulCenterFrequency, uint32_t ulSampleRate, const char *pszFileName)
{
    static uint32_t ulCurrentFFTSample = 0;
    static iq16_t* pFFTInput = NULL;
    static iq16_t* pFFTOutput = NULL;

    if(!ulCurrentFFTSample)
    {
        pFFTInput = (iq16_t *)malloc(ulFFTSize * sizeof(iq16_t));

        if(!pFFTInput)
            return 0;

        pFFTOutput = (iq16_t *)malloc(ulFFTSize * sizeof(iq16_t));

        if(!pFFTOutput)
            return 0;
    }

    if(ulCurrentFFTSample < ulFFTSize)
    {
        pFFTInput[ulCurrentFFTSample].i = xSample.i;
        pFFTInput[ulCurrentFFTSample].q = xSample.q;

        ulCurrentFFTSample++;

        return 0;
    }
    else
    {
        kiss_fft_cfg xFFT = kiss_fft_alloc(ulFFTSize, 0, NULL, NULL);

        if(!xFFT)
            return 0;

        kiss_fft(xFFT, (kiss_fft_cpx *)pFFTInput, (kiss_fft_cpx *)pFFTOutput);

        FILE *pFFTFile = fopen(pszFileName, "w");

        for(uint32_t i = 0; i < ulFFTSize; i++)
        {
            double gReal = (double)pFFTOutput[i].i / INT16_MAX;
            double gImag = (double)pFFTOutput[i].q / INT16_MAX;

            double gPower = 10 * log10(gReal * gReal + gImag * gImag);
            double gFrequency = 0;

            if(i < ulFFTSize / 2)
                gFrequency = (ulCenterFrequency + i * ((double)ulSampleRate / ulFFTSize));
            else
                gFrequency = (ulCenterFrequency + ((int32_t)i - (int32_t)ulFFTSize) * ((double)ulSampleRate / ulFFTSize));

            fprintf(pFFTFile, "%.6f\t%.2f\t%u\t%.6f\t%.6f\r\n", gFrequency / 1000, gPower, i, (double)pFFTInput[i].i / INT16_MAX, (double)pFFTInput[i].q / INT16_MAX);
        }

        fclose(pFFTFile);
        kiss_fft_free(xFFT);
        free(pFFTInput);
        free(pFFTOutput);

        ulCurrentFFTSample = 0;

        return 1;
    }
}

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
    static iq16_t xPrevSample = {0, 0}; // Keep the previous sample to calculate angle diff

    // Here we multiply the current sample by the conjugate of the previous to get the angle diff
    // instead of keeping the previous angle and differentiating after arctan
    // this saves us from unwrapping the angle
    iq16_t xDiff = IQ16_PRODUCT(xSample, IQ16_CONJUGATE(xPrevSample));

    xPrevSample = xSample;

    // Demodulate by getting the angle from the diff vector
    return atan2_int16(xDiff.q, xDiff.i);
}
int16_t am_demod(iq16_t xSample)
{
    // Amplitude is just the magnitude of the IQ vector

    return sqrt(xSample.i * xSample.i + xSample.q * xSample.q);
}

uint8_t stereo_audio_demod(int16_t sBaseband, int16_t sStereoPilot, int16_t *psLeft, int16_t *psRight)
{
    if(!psLeft)
        return 0;

    if(!psRight)
        return 0;

    // Mono audio (Left + Right)
    int16_t sMonoAudio = sBaseband; // Mono audio is the L+R component, let it simply be the baseband, will be LPF filtered later

    sMonoAudio *= 0.7; // Attenuate to increase stereo ratio (TODO: AGC)

    // Stereo audio (Left - Right)
    int16_t sStereoAudio = sBaseband;

    sStereoAudio = fir_filter(pStereoBandPass, sStereoAudio); // Bandpass the L-R (38k) subcarrier

    sStereoAudio = ((int32_t)sStereoAudio * sStereoPilot) / INT8_MAX; // Bring it to baseband, will be LPF filtered later to remove 2*38k component

    // Downsample
    uint8_t ubMonoDownsample = int16_downsample(pAudioDownsampler[0], sMonoAudio, &sMonoAudio);
    uint8_t ubStereoDownsample = int16_downsample(pAudioDownsampler[1], sStereoAudio, &sStereoAudio);

    if(ubMonoDownsample != 2 || ubStereoDownsample != 2) // Only proceed if both samples are ready (both should always be in sync)
        return 0;

    // Filter
    sMonoAudio = fir_filter(pAudioLowPass[0], sMonoAudio);
    sStereoAudio = fir_filter(pAudioLowPass[1], sStereoAudio);

    // Mix
    *psLeft = sMonoAudio + sStereoAudio; // (L + R) + (L - R) = L + R + L - R = 2L
    *psRight = sMonoAudio - sStereoAudio; // (L + R) - (L - R) = L + R - L + R = 2R

    return 1;
}
uint8_t rds_demod(int16_t sBaseband, int16_t sRDSPilot, uint8_t *pubBit)
{
    if(!pubBit)
        return 0;

    int16_t sRDS = sBaseband;

    sRDS = fir_filter(pRDSBandPass, sRDS);

    sRDS *= 20; // Gain (TODO: AGC)

    sRDS = ((int32_t)sRDS * sRDSPilot) / INT8_MAX;

    sRDS = fir_filter(pRDSLowPass, sRDS);

    static uint8_t phs_locked = 0;
    static int16_t prev_s = 0;

    if(!phs_locked)
    {
        if(sRDS > 0 && prev_s < 0)
        {
            printf("RDS LO phase locked!\r\n");

            pRDSLO->ulCurrentPhase = 0;

            phs_locked = 1;
        }

        prev_s = sRDS;

        return 0;
    }

    iq16_t xLO = oscillator_get(pRDSLO, 0);

    if(xLO.q < 0)
        sRDS = -sRDS;

    ////////////////////////////////////////////////////////////////////////
    static uint8_t done = 0;

    if(!done)
    {
        iq16_t in = {
            .i = sRDS,
            .q = 0
        };

        done = util_fft(in, 4096, 0, CHANNEL_BANDWIDTH, "./out/rds.txt");
    }
    ////////////////////////////////////////////////////////////////////////

    static int64_t acc = 0;
    static uint16_t acc_cnt = 0;

    acc += sRDS;
    acc_cnt++;

    uint8_t bit = 2;
    static int16_t prev_lo = 0;

    if(xLO.q >= 0 && prev_lo < 0)
    {
        //printf("Accumulated %hu samples\r\n", acc_cnt);

        bit = (acc / acc_cnt) > 0;

        acc = 0;
        acc_cnt = 0;
    }

    prev_lo = xLO.q;

    if(bit == 2)
        return 0;

    static uint8_t prev_bit = 0;

    prev_bit = prev_bit ^ bit;

    *pubBit = prev_bit;

    return 1;

    /*
    // Downsample
    if(int16_downsample(pRDSBBDownsampler, sRDS, &sRDS) != 2)
        return 0;

    if(int16_downsample(pRDSBitDownsampler, sRDS, &sRDS) != 2)
        return 0;

    uint8_t xbit = sRDS > 0;

    // Quadrature demod
    iq16_t xRDS = {
        .i = sRDS,
        .q = sRDS
    };

    iq16_t xLO = oscillator_get(pRDSLO, 0);

    // Multiply, this gives us the complex baseband signal
    xRDS = IQ16_PRODUCT(xRDS, xLO);
    xRDS = IQ16_SCALAR_QUOTIENT(xRDS, INT8_MAX);

    // Now get the phase and decimate to fBitRate
    //int16_t ang = atan2_int16(xRDS.q, xRDS.i);

    //float fang = (float)ang * 360.f / (float)TAU;

    uint8_t xbit = xRDS.i > 0;

    static uint8_t count = 0;
    static uint8_t acc = 0;

    count++;
    acc += xbit;

    if(count == 10) // fSample is fBitRate * 10
    {
        xbit = round((float)acc / count); // Majority vote

        acc = 0;
        count = 0;
    }
    else
        return 0;


    */
   return 0;
}

void channel_handler(iq16_t xSample)
{
    int16_t sBaseband = fm_demod(xSample); // FM demodulate, signal is now real

    fwrite(&sBaseband, sizeof(int16_t), 1, pBasebandFile); // Write the baseband data

    // 19 kHz pilot and multiples
    int16_t sPilot19k = sBaseband;

    sPilot19k = fir_filter(pPilot19kBandPass, sPilot19k);

    sPilot19k *= 0.4; // Attenuate to lower phase noise (TODO: AGC)

    //// Stereo Pilot
    //// 19 ^ 2 = 38, HPF to remove uwanted spectrum
    int16_t sPilot38k = sPilot19k;

    for(uint8_t i = 0; i < 1; i++) // if taking x^n, then loop until i < n - 1
        sPilot38k = ((int32_t)sPilot38k * sPilot19k) / INT8_MAX;

    sPilot38k = fir_filter(pPilot38kHighPass, sPilot38k);

    //// RDS Pilot
    //// 19 ^ 3 = 57, HPF to remove unwanted spectrum
    int16_t sPilot57k = sPilot19k;

    for(uint8_t i = 0; i < 2; i++) // if taking x^n, then loop until i < n - 1
        sPilot57k = ((int32_t)sPilot57k * sPilot19k) / INT8_MAX;

    sPilot57k = fir_filter(pPilot57kHighPass, sPilot57k);

    // Audio
    int16_t sLeft, sRight;

    if(stereo_audio_demod(sBaseband, sPilot38k, &sLeft, &sRight))
    {
        int32_t lMP3Write = lame_encode_buffer(xMP3Encoder, &sLeft, &sRight, 1, pMP3Buffer, MP3_BUFFER_SIZE); // Encode MP3

        if(lMP3Write > 0)
            fwrite(pMP3Buffer, sizeof(uint8_t), lMP3Write, pAudioFile);
    }

    // RDS
    uint8_t ubBit;

    if(rds_demod(sBaseband, sPilot57k, &ubBit))
    {
        char c = ubBit ? '1' : '0';

        fwrite(&c, sizeof(char), 1, pRDSFile);
        // Add RDS frame sync/decode
    }
}

void sample_handler(iq16_t xSample)
{
    iq16_t xLO = oscillator_get(pChannelLO, 0); // Get the current LO sample

    xSample = IQ16_PRODUCT(xSample, xLO); // Center the wanted channel back to 0
    xSample = IQ16_SCALAR_QUOTIENT(xSample, INT8_MAX);

    // Squelch
    static uint8_t ubSquelchLevel = 0; // Acts like a counter that keeps increasing if power at the carrier is below the threashold, and decreases otherwise
    static uint32_t ulCurrentFFTSample = 0;

    if(ulCurrentFFTSample < SQUELCH_FFT_SIZE)
    {
        pSquelchFFTInput[ulCurrentFFTSample].i = xSample.i * 128; // Scale up to completely fill an int16
        pSquelchFFTInput[ulCurrentFFTSample].q = xSample.q * 128; // Scale up to completely fill an int16

        ulCurrentFFTSample++;
    }
    else
    {
        kiss_fft(xSquelchFFT, (kiss_fft_cpx *)pSquelchFFTInput, (kiss_fft_cpx *)pSquelchFFTOutput);

        double gCarrierPower = -INFINITY;
        double gMaxPower = -INFINITY;
        double gMaxPowerFrequency = 0.f;

        for(uint32_t i = 0; i < SQUELCH_FFT_SIZE; i++)
        {
            double gReal = (double)pSquelchFFTOutput[i].i / INT16_MAX;
            double gImag = (double)pSquelchFFTOutput[i].q / INT16_MAX;

            double gPower = 10 * log10(gReal * gReal + gImag * gImag);
            double gFrequency = 0;

            if(i < SQUELCH_FFT_SIZE / 2)
                gFrequency = (CHANNEL_CARRIER + i * ((double)SAMPLE_RATE / SQUELCH_FFT_SIZE));
            else
                gFrequency = (CHANNEL_CARRIER + ((int32_t)i - SQUELCH_FFT_SIZE) * ((double)SAMPLE_RATE / SQUELCH_FFT_SIZE));

            if(i == 0) // Wanted channel is cented at 0 from the previous mix
                gCarrierPower = gPower;

            if(gPower > gMaxPower)
            {
                gMaxPower = gPower;
                gMaxPowerFrequency = gFrequency;
            }
        }

        //if(gMaxPowerFrequency < 104350000 && gMaxPowerFrequency > 104250000)
        //    DBGPRINTLN_CTX("Peak power = %.2f dB at frequency %.2f MHz", gMaxPower, gMaxPowerFrequency / 1000000);

        //DBGPRINTLN_CTX("Carrier power = %.2f dB", gCarrierPower);

        if(gCarrierPower < SQUELCH_LEVEL)
        {
            if(ubSquelchLevel < 10)
                ubSquelchLevel++;
        }
        else
        {
            if(ubSquelchLevel > 0)
                ubSquelchLevel--;
        }

        ulCurrentFFTSample = 0;
    }

    if(ubSquelchLevel > 3)
        xSample.i = xSample.q = 0;

    // LPF to reduce aliasing from nearby channels after downsampling
    fir_put(pBasebandLowPass[0], xSample.i);
    fir_put(pBasebandLowPass[1], xSample.q);

    if(iq16_downsample(pBasebandDownsampler, xSample, &xSample) != 2) // Downsample to the channel bandwidth
        return;

    xSample.i = fir_get(pBasebandLowPass[0]);
    xSample.q = fir_get(pBasebandLowPass[1]);

    channel_handler(xSample);
}

void signal_handler(int iSignal)
{
    DBGPRINTLN_CTX("Got signal %d, stop sampling...", iSignal);

    driver_rtlsdr_sample_stop(pSDR);
}

int main(int argc, char **argv)
{
    int32_t lOptChar;

    while((lOptChar = getopt(argc, argv, "p")) != -1)
    {
        switch (lOptChar)
        {
            case 'p':
                pBasebandFile = stdout;
            break;
        }
    }

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    DBGPRINTLN_CTX("Set SIGINT handler...");
    signal(SIGINT, signal_handler);

    DBGPRINTLN_CTX("Init audio components...");

    DBGPRINTLN_CTX("  Init MP3 encoder");

    xMP3Encoder = lame_init();

    lame_set_in_samplerate(xMP3Encoder, REAL_AUDIO_SAMPLE_RATE);
    lame_set_num_channels(xMP3Encoder, 2);
    lame_set_brate(xMP3Encoder, 320);
    lame_set_quality(xMP3Encoder, 2);
    lame_init_params(xMP3Encoder);

    DBGPRINTLN_CTX("  Alloc %u bytes for MP3 buffer", MP3_BUFFER_SIZE);

    pMP3Buffer = (uint8_t *)malloc(MP3_BUFFER_SIZE);

    if(!pMP3Buffer)
        return 1;

    DBGPRINTLN_CTX("  Setting audio LPF cutoff at %.2f kHz", (float)AUDIO_LPF_CUTOFF / 1000.f);

    pAudioLowPass[0] = fir_init(audio_lpf_coefs, AUDIO_LPF_TAP_NUM);

    if(!pAudioLowPass[0])
        return 1;

    pAudioLowPass[1] = fir_init(audio_lpf_coefs, AUDIO_LPF_TAP_NUM);

    if(!pAudioLowPass[1])
        return 1;

    DBGPRINTLN_CTX("  Setting audio downsample factor to %u, audio sample rate %.2f kHz", AUDIO_DOWNSAMPLE, (float)REAL_AUDIO_SAMPLE_RATE / 1000.f);

    pAudioDownsampler[0] = int16_downsampler_init(AUDIO_DOWNSAMPLE);

    if(!pAudioDownsampler[0])
        return 1;

    pAudioDownsampler[1] = int16_downsampler_init(AUDIO_DOWNSAMPLE);

    if(!pAudioDownsampler[1])
        return 1;

    DBGPRINTLN_CTX("Init RDS components...");

    DBGPRINTLN_CTX("  Setting RDS LPF cutoff at %.2f kHz", (float)RDS_LPF_CUTOFF / 1000.f);

    pRDSLowPass = fir_init(rds_lpf_coefs, RDS_LPF_TAP_NUM);

    if(!pRDSLowPass)
        return 1;

    DBGPRINTLN_CTX("  Setting RDS baseband downsample factor to %u, RDS sample rate %.1f Hz", RDS_BB_DOWNSAMPLE, (float)REAL_RDS_BB_SAMPLE_RATE);

    pRDSBBDownsampler = int16_downsampler_init(RDS_BB_DOWNSAMPLE);

    if(!pRDSBBDownsampler)
        return 1;

    DBGPRINTLN_CTX("  Setting RDS bit downsample factor to %u, RDS bit rate %.1f bps", RDS_BIT_DOWNSAMPLE, (float)REAL_RDS_BIT_SAMPLE_RATE);

    pRDSBitDownsampler = int16_downsampler_init(RDS_BIT_DOWNSAMPLE);

    if(!pRDSBitDownsampler)
        return 1;

    DBGPRINTLN_CTX("  Setting RDS LO frequency to %.1f Hz", (float)RDS_BITRATE);

    pRDSLO = oscillator_init(CHANNEL_BANDWIDTH, RDS_BITRATE);

    if(!pRDSLO)
        return 1;

    DBGPRINTLN_CTX("Init baseband components...");

    DBGPRINTLN_CTX("  Init Squelch FFT");

    xSquelchFFT = kiss_fft_alloc(SQUELCH_FFT_SIZE, 0, NULL, NULL);

    if(!xSquelchFFT)
        return 1;

    DBGPRINTLN_CTX("  Alloc %lu bytes for squelch FFT input buffer", SQUELCH_FFT_SIZE * sizeof(iq16_t));

    pSquelchFFTInput = (iq16_t *)malloc(SQUELCH_FFT_SIZE * sizeof(iq16_t));

    if(!pSquelchFFTInput)
        return 1;

    DBGPRINTLN_CTX("  Alloc %lu bytes for squelch FFT output buffer", SQUELCH_FFT_SIZE * sizeof(iq16_t));

    pSquelchFFTOutput = (iq16_t *)malloc(SQUELCH_FFT_SIZE * sizeof(iq16_t));

    if(!pSquelchFFTOutput)
        return 1;

    DBGPRINTLN_CTX("  Setting baseband LPF cutoff at %.2f kHz", (float)BB_LPF_CUTOFF / 1000.f);

    pBasebandLowPass[0] = fir_init(bb_lpf_coefs, BB_LPF_TAP_NUM);

    if(!pBasebandLowPass[0])
        return 1;

    pBasebandLowPass[1] = fir_init(bb_lpf_coefs, BB_LPF_TAP_NUM);

    if(!pBasebandLowPass[1])
        return 1;

    DBGPRINTLN_CTX("  Setting baseband downsample factor to %u, channel sample rate %.2f kHz", BB_DOWNSAMPLE, (float)REAL_CHANNEL_SAMPLE_RATE / 1000.f);

    pBasebandDownsampler = iq16_downsampler_init(BB_DOWNSAMPLE);

    if(!pBasebandDownsampler)
        return 1;

    DBGPRINTLN_CTX("  Setting 19 kHz pilot BPF low cutoff at %.2f kHz and high cutoff at %.2f kHz", (float)PILOT_19KHZ_BPF_LOW_CUTOFF / 1000.f,  (float)PILOT_19KHZ_BPF_HIGH_CUTOFF / 1000.f);

    pPilot19kBandPass = fir_init(pilot_19khz_bpf_coefs, PILOT_19KHZ_BPF_TAP_NUM);

    if(!pPilot19kBandPass)
        return 1;

    DBGPRINTLN_CTX("  Setting 38 kHz pilot HPF high cutoff at %.2f kHz", (float)PILOT_38KHZ_HPF_CUTOFF / 1000.f);

    pPilot38kHighPass = fir_init(pilot_38khz_hpf_coefs, PILOT_38KHZ_HPF_TAP_NUM);

    if(!pPilot38kHighPass)
        return 1;

    DBGPRINTLN_CTX("  Setting 57 kHz pilot HPF high cutoff at %.2f kHz", (float)PILOT_57KHZ_HPF_CUTOFF / 1000.f);

    pPilot57kHighPass = fir_init(pilot_57khz_hpf_coefs, PILOT_57KHZ_HPF_TAP_NUM);

    if(!pPilot57kHighPass)
        return 1;

    DBGPRINTLN_CTX("  Setting stereo BPF low cutoff at %.2f kHz and high cutoff at %.2f kHz", (float)STEREO_BPF_LOW_CUTOFF / 1000.f,  (float)STEREO_BPF_HIGH_CUTOFF / 1000.f);

    pStereoBandPass = fir_init(stereo_bpf_coefs, STEREO_BPF_TAP_NUM);

    if(!pStereoBandPass)
        return 1;

    DBGPRINTLN_CTX("  Setting RDS BPF low cutoff at %.2f kHz and high cutoff at %.2f kHz", (float)RDS_BPF_LOW_CUTOFF / 1000.f,  (float)RDS_BPF_HIGH_CUTOFF / 1000.f);

    pRDSBandPass = fir_init(rds_bpf_coefs, RDS_BPF_TAP_NUM);

    if(!pRDSBandPass)
        return 1;

    DBGPRINTLN_CTX("  Setting baseband LO frequency to %.2f kHz", (float)LO_FREQUENCY / 1000.f);

    pChannelLO = oscillator_init(SAMPLE_RATE, LO_FREQUENCY);

    if(!pChannelLO)
        return 1;

    DBGPRINTLN_CTX("Init output files...");

    if(!pBasebandFile)
    {
        pBasebandFile = fopen("./out/bb.raw", "w");

        if(!pBasebandFile)
            return 1;
    }

    pAudioFile = fopen("./out/audio_out.mp3", "w");

    if(!pAudioFile)
        return 1;

    pRDSFile = fopen("./out/rds.raw", "w");

    if(!pRDSFile)
        return 1;

    DBGPRINTLN_CTX("Init RTLSDR...");

    DBGPRINTLN_CTX("  Setting carrier to %.2f MHz", (float)CHANNEL_CARRIER / 1000000.f);
    DBGPRINTLN_CTX("  Tuning SDR to %.2f MHz", (float)SAMPLE_FREQUENCY / 1000000.f);
    DBGPRINTLN_CTX("  Setting SDR bandwidth to +/- %.2f kHz", (float)SAMPLE_RATE / 2.f / 1000.f);

    pSDR = driver_rtlsdr_init(SAMPLE_FREQUENCY, SAMPLE_RATE, 280, 0, sample_handler);

    if(!pSDR)
        return 1;

    DBGPRINTLN_CTX("Sample...");
    driver_rtlsdr_sample_start(pSDR);

    DBGPRINTLN_CTX("Deinit RTLSDR...");
    driver_rtlsdr_cleanup(pSDR);

    DBGPRINTLN_CTX("Deinit audio components...");

    DBGPRINTLN_CTX("  Flush MP3 encoder");

    int32_t lMP3Write = lame_encode_flush(xMP3Encoder, pMP3Buffer, MP3_BUFFER_SIZE);

    if(lMP3Write > 0)
        fwrite(pMP3Buffer, sizeof(uint8_t), lMP3Write, pAudioFile);

    lame_close(xMP3Encoder);
    free(pMP3Buffer);
    kiss_fft_free(xSquelchFFT);
    free(pSquelchFFTInput);
    free(pSquelchFFTOutput);
    fir_cleanup(pAudioLowPass[0]);
    fir_cleanup(pAudioLowPass[1]);
    int16_downsampler_cleanup(pAudioDownsampler[0]);
    int16_downsampler_cleanup(pAudioDownsampler[1]);

    DBGPRINTLN_CTX("Deinit output files...");
    fclose(pBasebandFile);
    fclose(pAudioFile);
    fclose(pRDSFile);

    DBGPRINTLN_CTX("Deinit baseband components...");
    fir_cleanup(pBasebandLowPass[0]);
    fir_cleanup(pBasebandLowPass[1]);
    fir_cleanup(pPilot19kBandPass);
    fir_cleanup(pPilot38kHighPass);
    fir_cleanup(pPilot57kHighPass);
    fir_cleanup(pStereoBandPass);
    fir_cleanup(pRDSBandPass);
    iq16_downsampler_cleanup(pBasebandDownsampler);
    oscillator_cleanup(pChannelLO);

    DBGPRINTLN_CTX("Deinit RDS components...");
    fir_cleanup(pRDSLowPass);
    int16_downsampler_cleanup(pRDSBBDownsampler);
    int16_downsampler_cleanup(pRDSBitDownsampler);
    oscillator_cleanup(pRDSLO);

    return 0;
}