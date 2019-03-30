#ifndef __FIR_H__
#define __FIR_H__

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*
    fSample: 1900000 Hz

    * 0 Hz - 190000 Hz
        gain = 1
        desired ripple = 1 dB
        actual ripple = 1 dB

    * 300000 Hz - 950000 Hz
        gain = 0
        desired attenuation = -50 dB
        actual attenuation = -50 dB
*/
#define BB_LPF_CUTOFF 300000
#define BB_LPF_TAP_NUM 35
static int16_t bb_lpf_coefs[BB_LPF_TAP_NUM] =
{
    -43,
    -213,
    -323,
    -376,
    -236,
    91,
    482,
    698,
    515,
    -115,
    -952,
    -1510,
    -1251,
    124,
    2463,
    5135,
    7257,
    8065,
    7257,
    5135,
    2463,
    124,
    -1251,
    -1510,
    -952,
    -115,
    515,
    698,
    482,
    91,
    -236,
    -376,
    -323,
    -213,
    -43
};

/*
    fSample: 47500 Hz

    * 0 Hz - 15000 Hz
        gain = 1
        desired ripple = 1 dB
        actual ripple = 1 dB

    * 19000 Hz - 25000 Hz
        gain = 0
        desired attenuation = -80 dB
        actual attenuation = -80 dB
*/
#define AUDIO_LPF_CUTOFF 19000
#define AUDIO_LPF_TAP_NUM 31
static int16_t audio_lpf_coefs[AUDIO_LPF_TAP_NUM] =
{
    -205,
    -279,
    385,
    404,
    -349,
    395,
    412,
    -837,
    725,
    513,
    -1731,
    1742,
    610,
    -4675,
    8663,
    22494,
    8663,
    -4675,
    610,
    1742,
    -1731,
    513,
    725,
    -837,
    412,
    395,
    -349,
    404,
    385,
    -279,
    -205
};

/*
    fSample: 190000 Hz

    * 0 Hz - 19000 Hz
        gain = 0
        desired attenuation = -60 dB
        actual attenuation = -60 dB

    * 24000 Hz - 52000 Hz
        gain = 1
        desired ripple = 1 dB
        actual ripple = 1 dB

    * 55500 Hz - 100000 Hz
        gain = 0
        desired attenuation = -50 dB
        actual attenuation = -50 dB
*/
#define STEREO_BPF_LOW_CUTOFF 19000
#define STEREO_BPF_HIGH_CUTOFF 55500
#define STEREO_BPF_TAP_NUM 101
static int16_t stereo_bpf_coefs[STEREO_BPF_TAP_NUM] =
{
    -99,
    31,
    233,
    111,
    -249,
    -255,
    71,
    139,
    -2,
    104,
    201,
    -79,
    -259,
    -47,
    5,
    -160,
    61,
    392,
    150,
    -165,
    8,
    -21,
    -449,
    -281,
    347,
    292,
    -14,
    340,
    379,
    -492,
    -725,
    3,
    14,
    -340,
    534,
    1228,
    107,
    -696,
    -5,
    -409,
    -1701,
    -384,
    1936,
    1088,
    -12,
    2040,
    1113,
    -5791,
    -7132,
    3385,
    10944,
    3385,
    -7132,
    -5791,
    1113,
    2040,
    -12,
    1088,
    1936,
    -384,
    -1701,
    -409,
    -5,
    -696,
    107,
    1228,
    534,
    -340,
    14,
    3,
    -725,
    -492,
    379,
    340,
    -14,
    292,
    347,
    -281,
    -449,
    -21,
    8,
    -165,
    150,
    392,
    61,
    -160,
    5,
    -47,
    -259,
    -79,
    201,
    104,
    -2,
    139,
    71,
    -255,
    -249,
    111,
    233,
    31,
    -99
};

/*
    fSample: 190000 Hz

    * 0 Hz - 53000 Hz
        gain = 0
        desired attenuation = -60 dB
        actual attenuation = -60 dB

    * 54500 Hz - 59500 Hz
        gain = 1
        desired ripple = 1 dB
        actual ripple = 1 dB

    * 61000 Hz - 100000 Hz
        gain = 0
        desired attenuation = -40 dB
        actual attenuation = -40 dB
*/
#define RDS_BPF_LOW_CUTOFF 53000
#define RDS_BPF_HIGH_CUTOFF 61000
#define RDS_BPF_TAP_NUM 271
static int16_t rds_bpf_coefs[RDS_BPF_TAP_NUM] =
{
    16,
    23,
    -66,
    79,
    -36,
    -21,
    31,
    11,
    -37,
    6,
    39,
    -27,
    -31,
    48,
    8,
    -61,
    27,
    53,
    -62,
    -21,
    81,
    -27,
    -71,
    71,
    31,
    -93,
    24,
    80,
    -71,
    -36,
    89,
    -17,
    -72,
    56,
    32,
    -64,
    7,
    45,
    -25,
    -19,
    18,
    6,
    -1,
    -18,
    -4,
    43,
    -21,
    -55,
    67,
    30,
    -108,
    34,
    109,
    -111,
    -54,
    159,
    -41,
    -145,
    134,
    68,
    -178,
    40,
    149,
    -126,
    -64,
    150,
    -28,
    -109,
    79,
    41,
    -70,
    4,
    26,
    5,
    2,
    -53,
    28,
    88,
    -112,
    -54,
    197,
    -63,
    -209,
    219,
    105,
    -325,
    93,
    303,
    -295,
    -137,
    396,
    -105,
    -334,
    308,
    137,
    -370,
    91,
    274,
    -230,
    -94,
    221,
    -43,
    -106,
    50,
    7,
    59,
    -41,
    -166,
    226,
    118,
    -449,
    156,
    515,
    -574,
    -266,
    906,
    -292,
    -896,
    948,
    415,
    -1367,
    429,
    1256,
    -1296,
    -542,
    1762,
    -548,
    -1537,
    1562,
    627,
    -2029,
    629,
    1694,
    -1703,
    -657,
    2123,
    -657,
    -1703,
    1694,
    629,
    -2029,
    627,
    1562,
    -1537,
    -548,
    1762,
    -542,
    -1296,
    1256,
    429,
    -1367,
    415,
    948,
    -896,
    -292,
    906,
    -266,
    -574,
    515,
    156,
    -449,
    118,
    226,
    -166,
    -41,
    59,
    7,
    50,
    -106,
    -43,
    221,
    -94,
    -230,
    274,
    91,
    -370,
    137,
    308,
    -334,
    -105,
    396,
    -137,
    -295,
    303,
    93,
    -325,
    105,
    219,
    -209,
    -63,
    197,
    -54,
    -112,
    88,
    28,
    -53,
    2,
    5,
    26,
    4,
    -70,
    41,
    79,
    -109,
    -28,
    150,
    -64,
    -126,
    149,
    40,
    -178,
    68,
    134,
    -145,
    -41,
    159,
    -54,
    -111,
    109,
    34,
    -108,
    30,
    67,
    -55,
    -21,
    43,
    -4,
    -18,
    -1,
    6,
    18,
    -19,
    -25,
    45,
    7,
    -64,
    32,
    56,
    -72,
    -17,
    89,
    -36,
    -71,
    80,
    24,
    -93,
    31,
    71,
    -71,
    -27,
    81,
    -21,
    -62,
    53,
    27,
    -61,
    8,
    48,
    -31,
    -27,
    39,
    6,
    -37,
    11,
    31,
    -21,
    -36,
    79,
    -66,
    23,
    16
};

/*
    fSample: 190000 Hz

    * 0 Hz - 16000 Hz
        gain = 0
        desired attenuation = -80 dB
        actual attenuation = -80 dB

    * 18500 Hz - 19500 Hz
        gain = 0.25
        desired ripple = 2 dB
        actual ripple = 2 dB

    * 22000 Hz - 100000 Hz
        gain = 0
        desired attenuation = -80 dB
        actual attenuation = -80 dB
*/
#define PILOT_19KHZ_BPF_LOW_CUTOFF 16000
#define PILOT_19KHZ_BPF_HIGH_CUTOFF 22000
#define PILOT_19KHZ_BPF_TAP_NUM 183
static int16_t pilot_19khz_bpf_coefs[PILOT_19KHZ_BPF_TAP_NUM] =
{
    1,
    1,
    2,
    0,
    -1,
    -2,
    -3,
    -3,
    -1,
    1,
    4,
    5,
    5,
    2,
    -2,
    -7,
    -10,
    -9,
    -4,
    4,
    11,
    16,
    14,
    6,
    -6,
    -18,
    -24,
    -21,
    -9,
    9,
    26,
    35,
    30,
    12,
    -13,
    -37,
    -48,
    -41,
    -17,
    18,
    49,
    64,
    55,
    22,
    -23,
    -63,
    -82,
    -70,
    -28,
    29,
    79,
    102,
    86,
    34,
    -35,
    -96,
    -123,
    -103,
    -41,
    42,
    113,
    144,
    120,
    47,
    -48,
    -130,
    -164,
    -136,
    -53,
    54,
    145,
    183,
    151,
    59,
    -60,
    -158,
    -199,
    -163,
    -63,
    64,
    169,
    211,
    172,
    66,
    -67,
    -175,
    -218,
    -177,
    -68,
    68,
    178,
    221,
    178,
    68,
    -68,
    -177,
    -218,
    -175,
    -67,
    66,
    172,
    211,
    169,
    64,
    -63,
    -163,
    -199,
    -158,
    -60,
    59,
    151,
    183,
    145,
    54,
    -53,
    -136,
    -164,
    -130,
    -48,
    47,
    120,
    144,
    113,
    42,
    -41,
    -103,
    -123,
    -96,
    -35,
    34,
    86,
    102,
    79,
    29,
    -28,
    -70,
    -82,
    -63,
    -23,
    22,
    55,
    64,
    49,
    18,
    -17,
    -41,
    -48,
    -37,
    -13,
    12,
    30,
    35,
    26,
    9,
    -9,
    -21,
    -24,
    -18,
    -6,
    6,
    14,
    16,
    11,
    4,
    -4,
    -9,
    -10,
    -7,
    -2,
    2,
    5,
    5,
    4,
    1,
    -1,
    -3,
    -3,
    -2,
    -1,
    0,
    2,
    1,
    1
};

/*
    fSample: 190000 Hz

    * 0 Hz - 20000 Hz
        gain = 0
        desired attenuation = -80 dB
        actual attenuation = -80 dB

    * 37000 Hz - 100000 Hz
        gain = 1
        desired ripple = 1 dB
        actual ripple = 1 dB
*/
#define PILOT_38KHZ_HPF_CUTOFF 37000
#define PILOT_38KHZ_HPF_TAP_NUM 29
static int16_t pilot_38khz_hpf_coefs[PILOT_38KHZ_HPF_TAP_NUM] =
{
    251,
    -536,
    -45,
    377,
    493,
    26,
    -734,
    -939,
    6,
    1543,
    2021,
    -37,
    -4422,
    -8951,
    21894,
    -8951,
    -4422,
    -37,
    2021,
    1543,
    6,
    -939,
    -734,
    26,
    493,
    377,
    -45,
    -536,
    251
};

/*
    fSample: 190000 Hz

    * 0 Hz - 20000 Hz
        gain = 0
        desired attenuation = -80 dB
        actual attenuation = -80 dB

    * 56000 Hz - 100000 Hz
        gain = 1
        desired ripple = 1 dB
        actual ripple = 1 dB
*/
#define PILOT_57KHZ_HPF_CUTOFF 56000
#define PILOT_57KHZ_HPF_TAP_NUM 15
static int16_t pilot_57khz_hpf_coefs[PILOT_57KHZ_HPF_TAP_NUM] =
{
    141,
    -21,
    -1026,
    736,
    2474,
    -1074,
    -10057,
    17657,
    -10057,
    -1074,
    2474,
    736,
    -1026,
    -21,
    141
};

typedef struct fir_filter_t
{
    int16_t *psCoefs;
    int16_t *psData;
    uint32_t ulTaps;
    uint32_t ulLastIndex;
} fir_filter_t;

fir_filter_t *fir_init(int16_t *pusCoefs, uint32_t ulTaps);
void fir_cleanup(fir_filter_t *pFilter);
void fir_put(fir_filter_t *pFilter, int16_t sSample);
int16_t fir_get(fir_filter_t *pFilter);
static inline int16_t fir_filter(fir_filter_t *pFilter, int16_t sSample)
{
    fir_put(pFilter, sSample);

    return fir_get(pFilter);
}

#endif
