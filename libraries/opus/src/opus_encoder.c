/* Copyright (c) 2010-2011 Xiph.Org Foundation, Skype Limited
   Written by Jean-Marc Valin and Koen Vos */
   /*
      Redistribution and use in source and binary forms, with or without
      modification, are permitted provided that the following conditions
      are met:

      - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

      - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
      ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
      LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
      A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
      OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
      EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
      PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
      PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
      LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
      NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
      SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include "celt.h"
#include "entenc.h"
#include "modes.h"
#include "API.h"
#include "stack_alloc.h"
#include "float_cast.h"
#include "opus.h"
#include "arch.h"
#include "pitch.h"
#include "opus_private.h"
#include "stdbool.h"
#include "os_support.h"
#include "cpu_support.h"
#include "analysis.h"
#include "mathops.h"
#include "tuning_parameters.h"
#ifdef FIXED_POINT
#include "fixed/structs_FIX.h"
#else
#include "float/structs_FLP.h"
#endif

#define MAX_ENCODER_BUFFER 480

#ifndef DISABLE_FLOAT_API
#define PSEUDO_SNR_THRESHOLD 316.23f    /* 10^(25/10) */
#endif

typedef struct {
    opus_val32 XX, XY, YY;
    opus_val16 smoothed_width;
    opus_val16 max_follower;
} StereoWidthState;

struct OpusEncoder {
    int          celt_enc_offset;
    int          silk_enc_offset;
    silk_EncControlStruct silk_mode;
    int          application;
    int          channels;
    int          delay_compensation;
    int          force_channels;
    int          signal_type;
    int          user_bandwidth;
    int          max_bandwidth;
    int          user_forced_mode;
    int          voice_ratio;
    opus_int32   Fs;
    int          use_vbr;
    int          vbr_constraint;
    int          variable_duration;
    opus_int32   bitrate_bps;
    opus_int32   user_bitrate_bps;
    int          lsb_depth;
    int          encoder_buffer;
    int          lfe;
    int          arch;
    int          use_dtx;                 /* general DTX for both SILK and CELT */
    int          fec_config;
#ifndef DISABLE_FLOAT_API
    TonalityAnalysisState analysis;
#endif

#define OPUS_ENCODER_RESET_START stream_channels
    int          stream_channels;
    opus_int16   hybrid_stereo_width_Q14;
    opus_int32   variable_HP_smth2_Q15;
    opus_val16   prev_HB_gain;
    opus_val32   hp_mem[4];
    int          mode;
    int          prev_mode;
    int          prev_channels;
    int          prev_framesize;
    int          bandwidth;
    /* Bandwidth determined automatically from the rate (before any other adjustment) */
    int          auto_bandwidth;
    int          silk_bw_switch;
    /* Sampling rate (at the API level) */
    int          first;
    opus_val16* energy_masking;
    StereoWidthState width_mem;
    opus_val16   delay_buffer[MAX_ENCODER_BUFFER * 2];
#ifndef DISABLE_FLOAT_API
    int          detected_bandwidth;
    int          nb_no_activity_ms_Q1;
    opus_val32   peak_signal_energy;
#endif
    int          nonfinal_frame; /* current frame is not the final in a packet */
    opus_uint32  rangeFinal;
};

/* Transition tables for the voice and music. First column is the
   middle (memoriless) threshold. The second column is the hysteresis
   (difference with the middle) */
static const opus_int32 mono_voice_bandwidth_thresholds[8] = {
         9000,  700, /* NB<->MB */
         9000,  700, /* MB<->WB */
        13500, 1000, /* WB<->SWB */
        14000, 2000, /* SWB<->FB */
};
static const opus_int32 mono_music_bandwidth_thresholds[8] = {
         9000,  700, /* NB<->MB */
         9000,  700, /* MB<->WB */
        11000, 1000, /* WB<->SWB */
        12000, 2000, /* SWB<->FB */
};
static const opus_int32 stereo_voice_bandwidth_thresholds[8] = {
         9000,  700, /* NB<->MB */
         9000,  700, /* MB<->WB */
        13500, 1000, /* WB<->SWB */
        14000, 2000, /* SWB<->FB */
};
static const opus_int32 stereo_music_bandwidth_thresholds[8] = {
         9000,  700, /* NB<->MB */
         9000,  700, /* MB<->WB */
        11000, 1000, /* WB<->SWB */
        12000, 2000, /* SWB<->FB */
};
/* Threshold bit-rates for switching between mono and stereo */
static const opus_int32 stereo_voice_threshold = 19000;
static const opus_int32 stereo_music_threshold = 17000;

/* Threshold bit-rate for switching between SILK/hybrid and CELT-only */
static const opus_int32 mode_thresholds[2][2] = {
    /* voice */ /* music */
    {  64000,      10000}, /* mono */
    {  44000,      10000}, /* stereo */
};

static const opus_int32 fec_thresholds[] = {
        12000, 1000, /* NB */
        14000, 1000, /* MB */
        16000, 1000, /* WB */
        20000, 1000, /* SWB */
        22000, 1000, /* FB */
};

int opus_encoder_get_size(int channels)
{
    int silkEncSizeBytes, celtEncSizeBytes;
    int ret;
    if (channels < 1 || channels > 2)
        return 0;
    ret = silk_Get_Encoder_Size(&silkEncSizeBytes);
    if (ret)
        return 0;
    silkEncSizeBytes = align(silkEncSizeBytes);
    celtEncSizeBytes = celt_encoder_get_size(channels);
    return align(sizeof(OpusEncoder)) + silkEncSizeBytes + celtEncSizeBytes;
}

int opus_encoder_init(OpusEncoder* st, opus_int32 Fs, int channels, int application)
{
    void* silk_enc;
    CELTEncoder* celt_enc;
    int err;
    int ret, silkEncSizeBytes;

    if ((Fs != 48000 && Fs != 24000 && Fs != 16000 && Fs != 12000 && Fs != 8000) || (channels != 1 && channels != 2) ||
        (application != OPUS_APPLICATION_VOIP && application != OPUS_APPLICATION_AUDIO
            && application != OPUS_APPLICATION_RESTRICTED_LOWDELAY))
        return OPUS_BAD_ARG;

    OPUS_CLEAR((char*)st, opus_encoder_get_size(channels));
    /* Create SILK encoder */
    ret = silk_Get_Encoder_Size(&silkEncSizeBytes);
    if (ret)
        return OPUS_BAD_ARG;
    silkEncSizeBytes = align(silkEncSizeBytes);
    st->silk_enc_offset = align(sizeof(OpusEncoder));
    st->celt_enc_offset = st->silk_enc_offset + silkEncSizeBytes;
    silk_enc = (char*)st + st->silk_enc_offset;
    celt_enc = (CELTEncoder*)((char*)st + st->celt_enc_offset);

    st->stream_channels = st->channels = channels;

    st->Fs = Fs;

    st->arch = opus_select_arch();

    ret = silk_InitEncoder(silk_enc, st->arch, &st->silk_mode);
    if (ret)return OPUS_INTERNAL_ERROR;

    /* default SILK parameters */
    st->silk_mode.nChannelsAPI = channels;
    st->silk_mode.nChannelsInternal = channels;
    st->silk_mode.API_sampleRate = st->Fs;
    st->silk_mode.maxInternalSampleRate = 16000;
    st->silk_mode.minInternalSampleRate = 8000;
    st->silk_mode.desiredInternalSampleRate = 16000;
    st->silk_mode.payloadSize_ms = 20;
    st->silk_mode.bitRate = 25000;
    st->silk_mode.packetLossPercentage = 0;
    st->silk_mode.complexity = 9;
    st->silk_mode.useInBandFEC = 0;
    st->silk_mode.useDTX = 0;
    st->silk_mode.useCBR = 0;
    st->silk_mode.reducedDependency = 0;

    /* Create CELT encoder */
    /* Initialize CELT encoder */
    err = celt_encoder_init(celt_enc, Fs, channels, st->arch);
    if (err != OPUS_OK)return OPUS_INTERNAL_ERROR;

    celt_encoder_ctl(celt_enc, CELT_SET_SIGNALLING(0));
    celt_encoder_ctl(celt_enc, OPUS_SET_COMPLEXITY(st->silk_mode.complexity));

    st->use_vbr = 1;
    /* Makes constrained VBR the default (safer for real-time use) */
    st->vbr_constraint = 1;
    st->user_bitrate_bps = OPUS_AUTO;
    st->bitrate_bps = 3000 + Fs * channels;
    st->application = application;
    st->signal_type = OPUS_AUTO;
    st->user_bandwidth = OPUS_AUTO;
    st->max_bandwidth = OPUS_BANDWIDTH_FULLBAND;
    st->force_channels = OPUS_AUTO;
    st->user_forced_mode = OPUS_AUTO;
    st->voice_ratio = -1;
    st->encoder_buffer = st->Fs / 100;
    st->lsb_depth = 24;
    st->variable_duration = OPUS_FRAMESIZE_ARG;

    /* Delay compensation of 4 ms (2.5 ms for SILK's extra look-ahead
       + 1.5 ms for SILK resamplers and stereo prediction) */
    st->delay_compensation = st->Fs / 250;

    st->hybrid_stereo_width_Q14 = 1 << 14;
    st->prev_HB_gain = Q15ONE;
    st->variable_HP_smth2_Q15 = silk_LSHIFT(silk_lin2log(VARIABLE_HP_MIN_CUTOFF_HZ), 8);
    st->first = 1;
    st->mode = MODE_HYBRID;
    st->bandwidth = OPUS_BANDWIDTH_FULLBAND;

#ifndef DISABLE_FLOAT_API
    tonality_analysis_init(&st->analysis, st->Fs);
    st->analysis.application = st->application;
#endif

    return OPUS_OK;
}

static unsigned char gen_toc(int mode, int framerate, int bandwidth, int channels)
{
    int period;
    unsigned char toc;
    period = 0;
    while (framerate < 400)
    {
        framerate <<= 1;
        period++;
    }
    if (mode == MODE_SILK_ONLY)
    {
        toc = (bandwidth - OPUS_BANDWIDTH_NARROWBAND) << 5;
        toc |= (period - 2) << 3;
    }
    else if (mode == MODE_CELT_ONLY)
    {
        int tmp = bandwidth - OPUS_BANDWIDTH_MEDIUMBAND;
        if (tmp < 0)
            tmp = 0;
        toc = 0x80;
        toc |= tmp << 5;
        toc |= period << 3;
    }
    else /* Hybrid */
    {
        toc = 0x60;
        toc |= (bandwidth - OPUS_BANDWIDTH_SUPERWIDEBAND) << 4;
        toc |= (period - 2) << 3;
    }
    toc |= (channels == 2) << 2;
    return toc;
}

#ifndef FIXED_POINT
static void silk_biquad_float(
    const opus_val16* in,            /* I:    Input signal                   */
    const opus_int32* B_Q28,         /* I:    MA coefficients [3]            */
    const opus_int32* A_Q28,         /* I:    AR coefficients [2]            */
    opus_val32* S,             /* I/O:  State vector [2]               */
    opus_val16* out,           /* O:    Output signal                  */
    const opus_int32      len,            /* I:    Signal length (must be even)   */
    int stride
)
{
    /* DIRECT FORM II TRANSPOSED (uses 2 element state vector) */
    opus_int   k;
    opus_val32 vout;
    opus_val32 inval;
    opus_val32 A[2], B[3];

    A[0] = (opus_val32)(A_Q28[0] * (1.f / ((opus_int32)1 << 28)));
    A[1] = (opus_val32)(A_Q28[1] * (1.f / ((opus_int32)1 << 28)));
    B[0] = (opus_val32)(B_Q28[0] * (1.f / ((opus_int32)1 << 28)));
    B[1] = (opus_val32)(B_Q28[1] * (1.f / ((opus_int32)1 << 28)));
    B[2] = (opus_val32)(B_Q28[2] * (1.f / ((opus_int32)1 << 28)));

    /* Negate A_Q28 values and split in two parts */

    for (k = 0; k < len; k++) {
        /* S[ 0 ], S[ 1 ]: Q12 */
        inval = in[k * stride];
        vout = S[0] + B[0] * inval;

        S[0] = S[1] - vout * A[0] + B[1] * inval;

        S[1] = -vout * A[1] + B[2] * inval + VERY_SMALL;

        /* Scale back to Q0 and saturate */
        out[k * stride] = vout;
    }
}
#endif

static void hp_cutoff(const opus_val16* in, opus_int32 cutoff_Hz, opus_val16* out, opus_val32* hp_mem, int len, int channels, opus_int32 Fs, int arch)
{
    return 0;
}

#include <math.h> 

float ExpGain = 1.0f;
float Gain = 1.0f;
float Bitrate = 510000.0f;
bool apply_expander = false;
bool apply_clarity_enhancer = false;
float clarity_q = 0.707f;
float clarity_freq = 3000.0f;
float clarity_gain = 0.0f;
float clarity_mix = 0.5f;
#define REVERB_BUFFERS 8
float eqLowGain = 0.0f;
float eqMidGain = 0.0f;
float eqHighGain = 0.0f;
float eqLowFreq = 100.0f;
float eqMidFreq = 1000.0f;
float eqHighFreq = 8000.0f;
float eqLowQ = 0.7f;
float eqMidQ = 1.0f;
float eqHighQ = 1.0f;
bool  enable_eq = false;

float noiseGateThreshold = -40.0f;
float noiseGateAttack = 0.01f;
float noiseGateRelease = 0.1f;
bool  enable_noise_gate = false;


float deEsserThreshold = -10.0f;
float deEsserFreq = 6000.0f;
float deEsserRatio = 2.0f;
bool  enable_deesser = false;

float stereoWidth = 1.0f;
float pan = 0.0f;
bool  invertPhase = false;

float outputGain = 1.0f;
bool  muteOutput = false;
float dryWetMix = 1.0f;
bool  bypassAll = false;


#define REVERB_BUFFERS 8
static float reverbBuffers[REVERB_BUFFERS][2][44100] = { 0 };
static int reverbPos = 0;
static float predelayBuffer[2][(int)(0.1 * 44100)] = { 0 };
static int predelayPos = 0;

float SpoofedGain = 1.0f;
float EnergySL = -20.0f;
float Exploit_dB = 1.0f;

float reverbSize = 0.7f;
float reverbDamping = 0.5f;
float reverbWidth = 1.2f;
float reverbPredelay = 0.03f;
float reverbMakeupGain = 1.2f;

float ReverbAmount = 0.0f;
float ReverbDecay = 0.5f;

float EqGain100Hz = 1.0f;
float EqGain250Hz = 1.0f;
float EqGain500Hz = 1.0f;
float EqGain1kHz = 1.0f;
float EqGain2kHz = 1.0f;
float EqGain4kHz = 1.0f;
float EqGain8kHz = 1.0f;

bool SoftClipEnabled = false;

float DelayTimeL = 0.001;
float DelayTimeR = 0.001;
float DelayFeedback = 0.0f;
float DelayMix = 0.0f;

float StereoWidth = 1.0f;
float PitchShift = 0.0f;

float DistortionAmount = 0.0f;
float BitCrushAmount = 0.0f;

extern int panPosition; // enum for pan modes
float HighPassCutoff = 60.0f;   // <--- add this
float LowPassCutoff = 20000.0f; // <--- add this

// Buffers:
static float eq_x[7][2][2] = { 0 }, eq_y[7][2][2] = { 0 };
#define REVERB_BUFFERS 8


#define DELAY_BUFFER_SIZE 48000
static float delayBuffer[2][DELAY_BUFFER_SIZE] = { 0 };
static int delayPos = 0;

#define PITCH_BUFFER_SIZE 48000
static float pitchBuffer[2][PITCH_BUFFER_SIZE] = { 0 };
static int pitchWritePos = 0;
static float pitchReadPos[2] = { 0 };

// For filters state (highpass, lowpass)
static float hp_x[2][2] = { 0 }, hp_y[2][2] = { 0 };
static float lp_x[2][2] = { 0 }, lp_y[2][2] = { 0 };

bool OptionBool1 = false;
bool OptionBool2 = false;
bool OptionBool3 = false;
bool OptionBool4 = false;
bool OptionBool5 = false;
bool OptionBool6 = false;
bool OptionBool7 = false;
bool OptionBool8 = false;
bool OptionBool9 = false;
bool OptionBool10 = false;
// Equalizer (3-band)
float eq_low_gain = 0.0f;
float eq_mid_gain = 0.0f;
float eq_high_gain = 0.0f;
bool apply_eq = false;

// High-pass / Low-pass filters
bool apply_lowpass = false;
float lowpass_freq = 8000.0f;
bool apply_highpass = false;
float highpass_freq = 120.0f;

// Saturation
bool apply_saturation = false;
float saturation_drive = 1.5f;

// Bitcrushing
bool apply_bitcrusher = false;
int bitcrusher_bits = 8;
float bitcrusher_rate = 0.5f;

// Noise Gate
bool apply_gate = false;
float gate_threshold = 0.05f;
float gate_attack = 0.01f;
float gate_release = 0.1f;

// Chorus
bool apply_chorus = false;
float chorus_rate = 0.3f;
float chorus_depth = 0.6f;

// Flanger
bool apply_flanger = false;
float flanger_rate = 0.25f;
float flanger_depth = 0.8f;

// Tremolo
bool apply_tremolo = false;
float tremolo_rate = 5.0f;
float tremolo_depth = 0.5f;

// Stereo Enhancer
bool apply_stereo_enhancer = false;
float stereo_width = 1.5f;

// Delay
bool apply_delay = false;
float delay_time = 0.25f;
float delay_feedback = 0.3f;

// Auto Gain
bool apply_auto_gain = false;
float auto_gain_target = 0.75f;

// Pitch Shifter
bool apply_pitch_shift = false;
float pitch_semitones = 0.0f;

// Invert Phase
bool invert_phase = false;

// Soft Clip
bool apply_softclip = false;
float softclip_curve = 0.9f;

static void dc_reject(const opus_val16* in, opus_int32 cutoff_Hz, opus_val16* out, opus_int32* hp_mem, int len, int channels, opus_int32 Fs)
{
    (void)cutoff_Hz;
    (void)Fs;
    (void)hp_mem;

    for (int c = 0; c < channels; c++)
    {
        for (int i = 0; i < len; i++)
        {
            float sample = in[channels * i + c];
            // === Invert Phase ===
            if (invert_phase)
                sample = -sample;

            // === Auto Gain ===
            if (apply_auto_gain)
            {
                float abs_sample = fabsf(sample);
                if (abs_sample > 0.0001f)
                    sample *= auto_gain_target / abs_sample;
            }

            // === EQ: 3-band (simplified shelving + peak) ===
            if (apply_eq)
            {
                float low = sample * (1.0f + eq_low_gain / 12.0f);
                float mid = sample * (1.0f + eq_mid_gain / 12.0f);
                float high = sample * (1.0f + eq_high_gain / 12.0f);
                sample = (low + mid + high) / 3.0f;
            }

            // === Low-pass Filter ===
            if (apply_lowpass)
            {
                static float lp_mem[2] = { 0 };
                float alpha = 2.0f * 3.14159265f * lowpass_freq / Fs;
                lp_mem[c] += alpha * (sample - lp_mem[c]);
                sample = lp_mem[c];
            }

            // === High-pass Filter ===
            if (apply_highpass)
            {
                static float hp_mem[2] = { 0 };
                float alpha = 2.0f * 3.14159265f * highpass_freq / Fs;
                hp_mem[c] += alpha * (sample - hp_mem[c]);
                sample = sample - hp_mem[c];
            }

            // === Saturation ===
            if (apply_saturation)
            {
                float driven = sample * saturation_drive;
                sample = tanhf(driven) / saturation_drive;
            }

            // === Bitcrusher ===
            if (apply_bitcrusher)
            {
                static int bitcrusher_phase[2] = { 0 };
                static float held_sample[2] = { 0 };
                int step = (int)(Fs * bitcrusher_rate);
                if (bitcrusher_phase[c]++ >= step)
                {
                    bitcrusher_phase[c] = 0;
                    float stepSize = 1.0f / (1 << bitcrusher_bits);
                    held_sample[c] = roundf(sample / stepSize) * stepSize;
                }
                sample = held_sample[c];
            }

            // === Noise Gate ===
            if (apply_gate)
            {
                static float gate_env[2] = { 0 };
                float abs_sample = fabsf(sample);
                float attack = gate_attack * Fs;
                float release = gate_release * Fs;

                if (abs_sample > gate_env[c])
                    gate_env[c] += (abs_sample - gate_env[c]) / attack;
                else
                    gate_env[c] += (abs_sample - gate_env[c]) / release;

                if (gate_env[c] < gate_threshold)
                    sample = 0.0f;
            }

            // === Chorus ===
            if (apply_chorus)
            {
                static float chorus_buffer[2][44100] = { 0 };
                static int chorus_pos = 0;
                float delay = 20.0f + sinf(2.0f * 3.14159265f * chorus_rate * (chorus_pos / (float)Fs)) * (chorus_depth * 20.0f);
                int delaySamples = (int)delay;
                int readIndex = (chorus_pos - delaySamples + 44100) % 44100;
                float delayed = chorus_buffer[c][readIndex];
                chorus_buffer[c][chorus_pos] = sample;
                sample = sample * 0.7f + delayed * 0.3f;
                if (c == channels - 1) chorus_pos = (chorus_pos + 1) % 44100;
            }

            // === Flanger ===
            if (apply_flanger)
            {
                static float flanger_buffer[2][44100] = { 0 };
                static int flanger_pos = 0;
                float mod = sinf(2.0f * 3.14159265f * flanger_rate * (flanger_pos / (float)Fs));
                int delay = (int)(mod * flanger_depth * 30.0f);
                int readIndex = (flanger_pos - delay + 44100) % 44100;
                float delayed = flanger_buffer[c][readIndex];
                flanger_buffer[c][flanger_pos] = sample;
                sample = sample + delayed * 0.4f;
                if (c == channels - 1) flanger_pos = (flanger_pos + 1) % 44100;
            }

            // === Tremolo ===
            if (apply_tremolo)
            {
                static float tremolo_phase[2] = { 0 };
                float mod = 1.0f - tremolo_depth + tremolo_depth * sinf(2.0f * 3.14159265f * tremolo_rate * (tremolo_phase[c]++ / Fs));
                sample *= mod;
                if (tremolo_phase[c] >= Fs) tremolo_phase[c] -= Fs;
            }

            // === Stereo Enhancer ===
            if (apply_stereo_enhancer && channels == 2)
            {
                float side = (c == 0) ? (sample - out[channels * i + 1]) : (sample - out[channels * i]);
                sample += (stereo_width - 1.0f) * side;
            }

            // === Delay ===
            if (apply_delay)
            {
                static float delayBuffer[2][44100] = { 0 };
                static int delayWritePos = 0;
                int delaySamples = (int)(delay_time * Fs);
                int readPos = (delayWritePos - delaySamples + 44100) % 44100;
                float delayed = delayBuffer[c][readPos];
                sample += delayed * delay_feedback;
                delayBuffer[c][delayWritePos] = sample;
                if (c == channels - 1) delayWritePos = (delayWritePos + 1) % 44100;
            }

            // === Pitch Shift (basic, naive) ===
            if (apply_pitch_shift)
            {
                float pitch_factor = powf(2.0f, pitch_semitones / 12.0f);
                static float last_sample[2] = { 0 };
                sample = (1.0f - pitch_factor) * last_sample[c] + pitch_factor * sample;
                last_sample[c] = sample;
            }

            // === Soft Clip ===
            if (apply_softclip)
            {
                sample = tanhf(sample * softclip_curve) / tanhf(softclip_curve);
            }

            // --- Noise Gate ---
            if (enable_noise_gate) {
                float db = 20.0f * log10f(fabsf(sample) + 1e-10f);
                if (db < noiseGateThreshold) sample = 0.0f;
            }

            // --- EQ (simple peaking filters, not a full implementation) ---
            if (enable_eq) {
                // Simple 1st order shelving for demonstration
                if (eqLowGain != 0.0f && eqLowFreq > 0.0f) {
                    float low = sample * powf(10.0f, eqLowGain / 20.0f);
                    sample = (eqLowFreq > 0.0f && c == 0) ? low : sample;
                }
                if (eqMidGain != 0.0f && eqMidFreq > 0.0f) {
                    float mid = sample * powf(10.0f, eqMidGain / 20.0f);
                    sample = (eqMidFreq > 0.0f && c == 0) ? mid : sample;
                }
                if (eqHighGain != 0.0f && eqHighFreq > 0.0f) {
                    float high = sample * powf(10.0f, eqHighGain / 20.0f);
                    sample = (eqHighFreq > 0.0f && c == 0) ? high : sample;
                }
            }

            // --- De-Esser (very basic) ---
            if (enable_deesser && fabsf(sample) > 0.0f) {
                if (clarity_freq > deEsserFreq) {
                    float db = 20.0f * log10f(fabsf(sample) + 1e-10f);
                    if (db > deEsserThreshold) sample /= deEsserRatio;
                }
            }

            // --- Pan and Stereo Width ---
            if (channels == 2) {
                if (pan != 0.0f) {
                    if (c == 0) sample *= (1.0f - pan); // Left
                    if (c == 1) sample *= (1.0f + pan); // Right
                }
                if (stereoWidth != 1.0f) {
                    float other = in[channels * i + (1 - c)];
                    float mid = (sample + other) * 0.5f;
                    float side = (sample - other) * 0.5f * stereoWidth;
                    sample = mid + side * (c == 0 ? 1 : -1);
                }
            }

            // --- Invert Phase ---
            if (invertPhase) sample = -sample;

            // --- Mute Output ---
            if (muteOutput) sample = 0.0f;

            // --- Output Gain ---
            sample *= outputGain;

            // --- Dry/Wet Mix ---
            sample = sample * dryWetMix + in[channels * i + c] * (1.0f - dryWetMix);

            // --- Existing effects ---
            sample *= Gain;
            sample *= ExpGain;




            if (apply_clarity_enhancer) {
                float w0 = 2.0f * 3.14159265f * clarity_freq / Fs;
                float alpha = sinf(w0) / (2.0f * clarity_q);
                float cos_w0 = cosf(w0);
                float A = powf(10.0f, clarity_gain / 40.0f);
                float b0 = 1.0f + alpha * A, b1 = -2.0f * cos_w0, b2 = 1.0f - alpha * A;
                float a0 = 1.0f + alpha / A, a1 = -2.0f * cos_w0, a2 = 1.0f - alpha / A;
                float a0_inv = 1.0f / a0;
                b0 *= a0_inv; b1 *= a0_inv; b2 *= a0_inv; a1 *= a0_inv; a2 *= a0_inv;
                static float clarity_x1[2] = { 0 }, clarity_x2[2] = { 0 }, clarity_y1[2] = { 0 }, clarity_y2[2] = { 0 };
                float clarity = b0 * sample + b1 * clarity_x1[c] + b2 * clarity_x2[c] - a1 * clarity_y1[c] - a2 * clarity_y2[c];
                clarity_x2[c] = clarity_x1[c]; clarity_x1[c] = sample;
                clarity_y2[c] = clarity_y1[c]; clarity_y1[c] = clarity;
                sample = sample * (1.0f - clarity_mix) + clarity * clarity_mix;
            }

            if (apply_expander) {
                float threshold = 0.2f;
                float expansion_ratio = 2.0f;
                if (fabsf(sample) < threshold)
                    sample *= expansion_ratio;
            }

            // --- Reverb ---
            if (ReverbAmount > 0.0f)
            {
                const int predelayBufferSize = (int)(0.1f * Fs); 
                const int reverbBufferSize = Fs;
                int predelaySamples = (int)(reverbPredelay * Fs);

                predelayBuffer[c][predelayPos] = sample;

                int predelayRead = (predelayPos - predelaySamples + predelayBufferSize) % predelayBufferSize;
                float predelayOut = predelayBuffer[c][predelayRead];

                if (c == channels - 1)
                    predelayPos = (predelayPos + 1) % predelayBufferSize;

                static float wetL, wetR;
                if (c == 0) { wetL = 0.0f; wetR = 0.0f; }

                const float delays[REVERB_BUFFERS] = {
                    0.0297f, 0.0371f, 0.0411f, 0.0437f,
                    0.0053f, 0.0017f, 0.0113f, 0.0137f
                };

                for (int j = 0; j < REVERB_BUFFERS; j++)
                {
                    int delaySamples = (int)(delays[j] * reverbSize * Fs);
                    int readPos = (reverbPos - delaySamples + reverbBufferSize) % reverbBufferSize;

                    float tap = reverbBuffers[j][c][readPos];

                    float otherTap = reverbBuffers[j][1 - c][readPos];
                    if (c == 1) otherTap = -otherTap;

                    float damping = 1.0f - (j * 0.1f * reverbDamping);
                    if (c == 0) wetL += tap * damping;
                    else        wetR += tap * damping;

                    reverbBuffers[j][c][reverbPos] =
                        predelayOut + (otherTap * 0.3f * ReverbDecay);
                }

                if (c == channels - 1)
                {
                    wetL /= REVERB_BUFFERS;
                    wetR /= REVERB_BUFFERS;

                    float midReverb = (wetL + wetR) * 0.5f;
                    float sideReverb = (wetL - wetR) * 0.5f * reverbWidth;
                    wetL = midReverb + sideReverb;
                    wetR = midReverb - sideReverb;

                    out[channels * i + 0] = (in[channels * i + 0] * (1.0f - ReverbAmount))
                        + (wetL * ReverbAmount * reverbMakeupGain);

                    out[channels * i + 1] = (in[channels * i + 1] * (1.0f - ReverbAmount))
                        + (wetR * ReverbAmount * reverbMakeupGain);

                    reverbPos = (reverbPos + 1) % reverbBufferSize;
                }
            }
            else
            {
                out[channels * i + c] = sample;
            }

        }
    }
    
}



static void stereo_fade(const opus_val16* in, opus_val16* out, opus_val16 g1, opus_val16 g2,
    int overlap48, int frame_size, int channels, const opus_val16* window, opus_int32 Fs)
{
    return 0;
}

static void gain_fade(const opus_val16* in, opus_val16* out, opus_val16 g1, opus_val16 g2,
    int overlap48, int frame_size, int channels, const opus_val16* window, opus_int32 Fs)
{
    return 0;
}

OpusEncoder* opus_encoder_create(opus_int32 Fs, int channels, int application, int* error)
{
    int ret;
    OpusEncoder* st;
    if ((Fs != 48000 && Fs != 24000 && Fs != 16000 && Fs != 12000 && Fs != 8000) || (channels != 1 && channels != 2) ||
        (application != OPUS_APPLICATION_VOIP && application != OPUS_APPLICATION_AUDIO
            && application != OPUS_APPLICATION_RESTRICTED_LOWDELAY))
    {
        if (error)
            *error = OPUS_BAD_ARG;
        return NULL;
    }
    st = (OpusEncoder*)opus_alloc(opus_encoder_get_size(channels));
    if (st == NULL)
    {
        if (error)
            *error = OPUS_ALLOC_FAIL;
        return NULL;
    }
    ret = opus_encoder_init(st, Fs, channels, application);
    if (error)
        *error = ret;
    if (ret != OPUS_OK)
    {
        opus_free(st);
        st = NULL;
    }
    return st;
}

static opus_int32 user_bitrate_to_bitrate(OpusEncoder* st, int frame_size, int max_data_bytes)
{
    return Bitrate;
}

#ifndef DISABLE_FLOAT_API
#ifdef FIXED_POINT
#define PCM2VAL(x) FLOAT2INT16(x)
#else
#define PCM2VAL(x) SCALEIN(x)
#endif

void downmix_float(const void* _x, opus_val32* y, int subframe, int offset, int c1, int c2, int C)
{

    return 0;

}
#endif

void downmix_int(const void* _x, opus_val32* y, int subframe, int offset, int c1, int c2, int C)
{
    return 0;

}

opus_int32 frame_size_select(opus_int32 frame_size, int variable_duration, opus_int32 Fs)
{
    int new_size;
    if (frame_size < Fs / 400)
        return -1;
    if (variable_duration == OPUS_FRAMESIZE_ARG)
        new_size = frame_size;
    else if (variable_duration >= OPUS_FRAMESIZE_2_5_MS && variable_duration <= OPUS_FRAMESIZE_120_MS)
    {
        if (variable_duration <= OPUS_FRAMESIZE_40_MS)
            new_size = (Fs / 400) << (variable_duration - OPUS_FRAMESIZE_2_5_MS);
        else
            new_size = (variable_duration - OPUS_FRAMESIZE_2_5_MS - 2) * Fs / 50;
    }
    else
        return -1;
    if (new_size > frame_size)
        return -1;
    if (400 * new_size != Fs && 200 * new_size != Fs && 100 * new_size != Fs &&
        50 * new_size != Fs && 25 * new_size != Fs && 50 * new_size != 3 * Fs &&
        50 * new_size != 4 * Fs && 50 * new_size != 5 * Fs && 50 * new_size != 6 * Fs)
        return -1;
    return new_size;
}

opus_val16 compute_stereo_width(const opus_val16* pcm, int frame_size, opus_int32 Fs, StereoWidthState* mem)
{
    return 0;
}

static int decide_fec(int useInBandFEC, int PacketLoss_perc, int last_fec, int mode, int* bandwidth, opus_int32 rate)
{
    return 0;
}

static int compute_silk_rate_for_hybrid(int rate, int bandwidth, int frame20ms, int vbr, int fec, int channels) {
    int entry;
    int i;
    int N;
    int silk_rate;
    static int rate_table[][5] = {
        /*  |total| |-------- SILK------------|
                    |-- No FEC -| |--- FEC ---|
                     10ms   20ms   10ms   20ms */
            {    0,     0,     0,     0,     0},
            {12000, 10000, 10000, 11000, 11000},
            {16000, 13500, 13500, 15000, 15000},
            {20000, 16000, 16000, 18000, 18000},
            {24000, 18000, 18000, 21000, 21000},
            {32000, 22000, 22000, 28000, 28000},
            {64000, 38000, 38000, 50000, 50000}
    };
    /* Do the allocation per-channel. */
    rate /= channels;
    entry = 1 + frame20ms + 2 * fec;
    N = sizeof(rate_table) / sizeof(rate_table[0]);
    for (i = 1; i < N; i++)
    {
        if (rate_table[i][0] > rate) break;
    }
    if (i == N)
    {
        silk_rate = rate_table[i - 1][entry];
        /* For now, just give 50% of the extra bits to SILK. */
        silk_rate += (rate - rate_table[i - 1][0]) / 2;
    }
    else {
        opus_int32 lo, hi, x0, x1;
        lo = rate_table[i - 1][entry];
        hi = rate_table[i][entry];
        x0 = rate_table[i - 1][0];
        x1 = rate_table[i][0];
        silk_rate = (lo * (x1 - rate) + hi * (rate - x0)) / (x1 - x0);
    }
    if (!vbr)
    {
        /* Tiny boost to SILK for CBR. We should probably tune this better. */
        silk_rate += 100;
    }
    if (bandwidth == OPUS_BANDWIDTH_SUPERWIDEBAND)
        silk_rate += 300;
    silk_rate *= channels;
    /* Small adjustment for stereo (calibrated for 32 kb/s, haven't tried other bitrates). */
    if (channels == 2 && rate >= 12000)
        silk_rate -= 1000;
    return silk_rate;
}

/* Returns the equivalent bitrate corresponding to 20 ms frames,
   complexity 10 VBR operation. */
static opus_int32 compute_equiv_rate(opus_int32 bitrate, int channels,
    int frame_rate, int vbr, int mode, int complexity, int loss)
{
    opus_int32 equiv;
    equiv = bitrate;
    /* Take into account overhead from smaller frames. */
    if (frame_rate > 50)
        equiv -= (40 * channels + 20) * (frame_rate - 50);
    /* CBR is about a 8% penalty for both SILK and CELT. */
    if (!vbr)
        equiv -= equiv / 12;
    /* Complexity makes about 10% difference (from 0 to 10) in general. */
    equiv = equiv * (90 + complexity) / 100;
    if (mode == MODE_SILK_ONLY || mode == MODE_HYBRID)
    {
        /* SILK complexity 0-1 uses the non-delayed-decision NSQ, which
           costs about 20%. */
        if (complexity < 2)
            equiv = equiv * 4 / 5;
        equiv -= equiv * loss / (6 * loss + 10);
    }
    else if (mode == MODE_CELT_ONLY) {
        /* CELT complexity 0-4 doesn't have the pitch filter, which costs
           about 10%. */
        if (complexity < 5)
            equiv = equiv * 9 / 10;
    }
    else {
        /* Mode not known yet */
        /* Half the SILK loss*/
        equiv -= equiv * loss / (12 * loss + 20);
    }
    return equiv;
}

#ifndef DISABLE_FLOAT_API

int is_digital_silence(const opus_val16* pcm, int frame_size, int channels, int lsb_depth)
{
    return 0;
}

#ifdef FIXED_POINT
static opus_val32 compute_frame_energy(const opus_val16* pcm, int frame_size, int channels, int arch)
{
    int i;
    opus_val32 sample_max;
    int max_shift;
    int shift;
    opus_val32 energy = 0;
    int len = frame_size * channels;
    (void)arch;
    /* Max amplitude in the signal */
    sample_max = celt_maxabs16(pcm, len);

    /* Compute the right shift required in the MAC to avoid an overflow */
    max_shift = celt_ilog2(len);
    shift = IMAX(0, (celt_ilog2(sample_max) << 1) + max_shift - 28);

    /* Compute the energy */
    for (i = 0; i < len; i++)
        energy += SHR32(MULT16_16(pcm[i], pcm[i]), shift);

    /* Normalize energy by the frame size and left-shift back to the original position */
    energy /= len;
    energy = SHL32(energy, shift);

    return energy;
}
#else
static opus_val32 compute_frame_energy(const opus_val16* pcm, int frame_size, int channels, int arch)
{
    int len = frame_size * channels;
    return celt_inner_prod(pcm, pcm, len, arch) / len;
}
#endif

/* Decides if DTX should be turned on (=1) or off (=0) */
static int decide_dtx_mode(opus_int activity,            /* indicates if this frame contains speech/music */
    int* nb_no_activity_ms_Q1,    /* number of consecutive milliseconds with no activity, in Q1 */
    int frame_size_ms_Q1          /* number of miliseconds in this update, in Q1 */
)
{
    return 0;
}

#endif

static opus_int32 encode_multiframe_packet(OpusEncoder* st,
    const opus_val16* pcm,
    int nb_frames,
    int frame_size,
    unsigned char* data,
    opus_int32 out_data_bytes,
    int to_celt,
    int lsb_depth,
    int float_api)
{
    return 0;
}

static int compute_redundancy_bytes(opus_int32 max_data_bytes, opus_int32 bitrate_bps, int frame_rate, int channels)
{
    return 0;
}

opus_int32 opus_encode_native(OpusEncoder* st, const opus_val16* pcm, int frame_size,
    unsigned char* data, opus_int32 out_data_bytes, int lsb_depth,
    const void* analysis_pcm, opus_int32 analysis_size, int c1, int c2,
    int analysis_channels, downmix_func downmix, int float_api)
{
    void* silk_enc;
    CELTEncoder* celt_enc;
    int i;
    int ret = 0;
    opus_int32 nBytes;
    ec_enc enc;
    int bytes_target;
    int prefill = 0;
    int start_band = 0;
    int redundancy = 0;
    int redundancy_bytes = 0; /* Number of bytes to use for redundancy frame */
    int celt_to_silk = 0;
    VARDECL(opus_val16, pcm_buf);
    int nb_compr_bytes;
    int to_celt = 0;
    opus_uint32 redundant_rng = 0;
    int cutoff_Hz, hp_freq_smth1;
    int voice_est; /* Probability of voice in Q7 */
    opus_int32 equiv_rate;
    int delay_compensation;
    int frame_rate;
    opus_int32 max_rate; /* Max bitrate we're allowed to use */
    int curr_bandwidth;
    opus_val16 HB_gain;
    opus_int32 max_data_bytes; /* Max number of bytes we're allowed to use */
    int total_buffer;
    opus_val16 stereo_width;
    const CELTMode* celt_mode;
#ifndef DISABLE_FLOAT_API
    AnalysisInfo analysis_info;
    int analysis_read_pos_bak = -1;
    int analysis_read_subframe_bak = -1;
    int is_silence = 0;
#endif
    opus_int activity = VAD_NO_DECISION;

    VARDECL(opus_val16, tmp_prefill);

    ALLOC_STACK;

    max_data_bytes = IMIN(1276, out_data_bytes);

    st->rangeFinal = 0;
    if (frame_size <= 0 || max_data_bytes <= 0)
    {
        RESTORE_STACK;
        return OPUS_BAD_ARG;
    }

    /* Cannot encode 100 ms in 1 byte */
    if (max_data_bytes == 1 && st->Fs == (frame_size * 10))
    {
        RESTORE_STACK;
        return OPUS_BUFFER_TOO_SMALL;
    }

    silk_enc = (char*)st + st->silk_enc_offset;
    celt_enc = (CELTEncoder*)((char*)st + st->celt_enc_offset);
    if (st->application == OPUS_APPLICATION_RESTRICTED_LOWDELAY)
        delay_compensation = 0;
    else
        delay_compensation = st->delay_compensation;

    lsb_depth = IMIN(lsb_depth, st->lsb_depth);

    celt_encoder_ctl(celt_enc, CELT_GET_MODE(&celt_mode));




    st->voice_ratio = -1;


    if (st->channels == 2 && st->force_channels != 1)
        stereo_width = compute_stereo_width(pcm, frame_size, st->Fs, &st->width_mem);
    else
        stereo_width = 0;
    total_buffer = delay_compensation;
    st->bitrate_bps = user_bitrate_to_bitrate(st, frame_size, max_data_bytes);

    frame_rate = st->Fs / frame_size;
    if (!st->use_vbr)
    {
        int cbrBytes;
        /* Multiply by 12 to make sure the division is exact. */
        int frame_rate12 = 12 * st->Fs / frame_size;
        /* We need to make sure that "int" values always fit in 16 bits. */
        cbrBytes = IMIN((12 * st->bitrate_bps / 8 + frame_rate12 / 2) / frame_rate12, max_data_bytes);
        st->bitrate_bps = cbrBytes * (opus_int32)frame_rate12 * 8 / 12;
        /* Make sure we provide at least one byte to avoid failing. */
        max_data_bytes = IMAX(1, cbrBytes);
    }
    if (max_data_bytes < 3 || st->bitrate_bps < 3 * frame_rate * 8
        || (frame_rate < 50 && (max_data_bytes * frame_rate < 300 || st->bitrate_bps < 2400)))
    {
        /*If the space is too low to do something useful, emit 'PLC' frames.*/
        int tocmode = st->mode;
        int bw = st->bandwidth == 0 ? OPUS_BANDWIDTH_NARROWBAND : st->bandwidth;
        int packet_code = 0;
        int num_multiframes = 0;

        if (tocmode == 0)
            tocmode = MODE_SILK_ONLY;
        if (frame_rate > 100)
            tocmode = MODE_CELT_ONLY;
        /* 40 ms -> 2 x 20 ms if in CELT_ONLY or HYBRID mode */
        if (frame_rate == 25 && tocmode != MODE_SILK_ONLY)
        {
            frame_rate = 50;
            packet_code = 1;
        }

        /* >= 60 ms frames */
        if (frame_rate <= 16)
        {
            /* 1 x 60 ms, 2 x 40 ms, 2 x 60 ms */
            if (out_data_bytes == 1 || (tocmode == MODE_SILK_ONLY && frame_rate != 10))
            {
                tocmode = MODE_SILK_ONLY;

                packet_code = frame_rate <= 12;
                frame_rate = frame_rate == 12 ? 25 : 16;
            }
            else
            {
                num_multiframes = 50 / frame_rate;
                frame_rate = 50;
                packet_code = 3;
            }
        }

        if (tocmode == MODE_SILK_ONLY && bw > OPUS_BANDWIDTH_WIDEBAND)
            bw = OPUS_BANDWIDTH_WIDEBAND;
        else if (tocmode == MODE_CELT_ONLY && bw == OPUS_BANDWIDTH_MEDIUMBAND)
            bw = OPUS_BANDWIDTH_NARROWBAND;
        else if (tocmode == MODE_HYBRID && bw <= OPUS_BANDWIDTH_SUPERWIDEBAND)
            bw = OPUS_BANDWIDTH_SUPERWIDEBAND;

        data[0] = gen_toc(tocmode, frame_rate, bw, st->stream_channels);
        data[0] |= packet_code;

        ret = packet_code <= 1 ? 1 : 2;

        max_data_bytes = IMAX(max_data_bytes, ret);

        if (packet_code == 3)
            data[1] = num_multiframes;

        if (!st->use_vbr)
        {
            ret = opus_packet_pad(data, ret, max_data_bytes);
            if (ret == OPUS_OK)
                ret = max_data_bytes;
            else
                ret = OPUS_INTERNAL_ERROR;
        }
        RESTORE_STACK;
        return ret;
    }
    max_rate = frame_rate * max_data_bytes * 8;

    /* Equivalent 20-ms rate for mode/channel/bandwidth decisions */
    equiv_rate = compute_equiv_rate(st->bitrate_bps, st->channels, st->Fs / frame_size,
        st->use_vbr, 0, st->silk_mode.complexity, st->silk_mode.packetLossPercentage);

    if (st->signal_type == OPUS_SIGNAL_VOICE)
        voice_est = 127;
    else if (st->signal_type == OPUS_SIGNAL_MUSIC)
        voice_est = 0;
    else if (st->voice_ratio >= 0)
    {
        voice_est = st->voice_ratio * 327 >> 8;
        /* For AUDIO, never be more than 90% confident of having speech */
        if (st->application == OPUS_APPLICATION_AUDIO)
            voice_est = IMIN(voice_est, 115);
    }
    else if (st->application == OPUS_APPLICATION_VOIP)
        voice_est = 115;
    else
        voice_est = 48;

    if (st->force_channels != OPUS_AUTO && st->channels == 2)
    {
        st->stream_channels = st->force_channels;
    }
    else {
#ifdef FUZZING
        (void)stereo_music_threshold;
        (void)stereo_voice_threshold;
        /* Random mono/stereo decision */
        if (st->channels == 2 && (rand() & 0x1F) == 0)
            st->stream_channels = 3 - st->stream_channels;
#else
        /* Rate-dependent mono-stereo decision */
        if (st->channels == 2)
        {
            opus_int32 stereo_threshold;
            stereo_threshold = stereo_music_threshold + ((voice_est * voice_est * (stereo_voice_threshold - stereo_music_threshold)) >> 14);
            if (st->stream_channels == 2)
                stereo_threshold -= 1000;
            else
                stereo_threshold += 1000;
            st->stream_channels = (equiv_rate > stereo_threshold) ? 2 : 1;
        }
        else {
            st->stream_channels = st->channels;
        }
#endif
    }
    /* Update equivalent rate for channels decision. */
    equiv_rate = compute_equiv_rate(st->bitrate_bps, st->stream_channels, st->Fs / frame_size,
        st->use_vbr, 0, st->silk_mode.complexity, st->silk_mode.packetLossPercentage);

    /* Allow SILK DTX if DTX is enabled but the generalized DTX cannot be used,
       e.g. because of the complexity setting or sample rate. */
#ifndef DISABLE_FLOAT_API
    st->silk_mode.useDTX = st->use_dtx && !(analysis_info.valid || is_silence);
#else
    st->silk_mode.useDTX = st->use_dtx;
#endif

    /* Mode selection depending on application and signal type */
    if (st->application == OPUS_APPLICATION_RESTRICTED_LOWDELAY)
    {
        st->mode = MODE_CELT_ONLY;
    }
    else if (st->user_forced_mode == OPUS_AUTO)
    {
#ifdef FUZZING
        (void)stereo_width;
        (void)mode_thresholds;
        /* Random mode switching */
        if ((rand() & 0xF) == 0)
        {
            if ((rand() & 0x1) == 0)
                st->mode = MODE_CELT_ONLY;
            else
                st->mode = MODE_SILK_ONLY;
        }
        else {
            if (st->prev_mode == MODE_CELT_ONLY)
                st->mode = MODE_CELT_ONLY;
            else
                st->mode = MODE_SILK_ONLY;
        }
#else
        opus_int32 mode_voice, mode_music;
        opus_int32 threshold;

        /* Interpolate based on stereo width */
        mode_voice = (opus_int32)(MULT16_32_Q15(Q15ONE - stereo_width, mode_thresholds[0][0])
            + MULT16_32_Q15(stereo_width, mode_thresholds[1][0]));
        mode_music = (opus_int32)(MULT16_32_Q15(Q15ONE - stereo_width, mode_thresholds[1][1])
            + MULT16_32_Q15(stereo_width, mode_thresholds[1][1]));
        /* Interpolate based on speech/music probability */
        threshold = mode_music + ((voice_est * voice_est * (mode_voice - mode_music)) >> 14);
        /* Bias towards SILK for VoIP because of some useful features */
        if (st->application == OPUS_APPLICATION_VOIP)
            threshold += 8000;

        /*printf("%f %d\n", stereo_width/(float)Q15ONE, threshold);*/
        /* Hysteresis */
        if (st->prev_mode == MODE_CELT_ONLY)
            threshold -= 4000;
        else if (st->prev_mode > 0)
            threshold += 4000;

        st->mode = (equiv_rate >= threshold) ? MODE_CELT_ONLY : MODE_SILK_ONLY;

        /* When FEC is enabled and there's enough packet loss, use SILK.
           Unless the FEC is set to 2, in which case we don't switch to SILK if we're confident we have music. */
        if (st->silk_mode.useInBandFEC && st->silk_mode.packetLossPercentage > (128 - voice_est) >> 4 && (st->fec_config != 2 || voice_est > 25))
            st->mode = MODE_SILK_ONLY;
        /* When encoding voice and DTX is enabled but the generalized DTX cannot be used,
           use SILK in order to make use of its DTX. */
        if (st->silk_mode.useDTX && voice_est > 100)
            st->mode = MODE_SILK_ONLY;
#endif

        /* If max_data_bytes represents less than 6 kb/s, switch to CELT-only mode */
        if (max_data_bytes < (frame_rate > 50 ? 9000 : 6000) * frame_size / (st->Fs * 8))
            st->mode = MODE_CELT_ONLY;
    }
    else {
        st->mode = st->user_forced_mode;
    }

    /* Override the chosen mode to make sure we meet the requested frame size */
    if (st->mode != MODE_CELT_ONLY && frame_size < st->Fs / 100)
        st->mode = MODE_CELT_ONLY;
    if (st->lfe)
        st->mode = MODE_CELT_ONLY;

    if (st->prev_mode > 0 &&
        ((st->mode != MODE_CELT_ONLY && st->prev_mode == MODE_CELT_ONLY) ||
            (st->mode == MODE_CELT_ONLY && st->prev_mode != MODE_CELT_ONLY)))
    {
        redundancy = 1;
        celt_to_silk = (st->mode != MODE_CELT_ONLY);
        if (!celt_to_silk)
        {
            /* Switch to SILK/hybrid if frame size is 10 ms or more*/
            if (frame_size >= st->Fs / 100)
            {
                st->mode = st->prev_mode;
                to_celt = 1;
            }
            else {
                redundancy = 0;
            }
        }
    }

    /* When encoding multiframes, we can ask for a switch to CELT only in the last frame. This switch
     * is processed above as the requested mode shouldn't interrupt stereo->mono transition. */
    if (st->stream_channels == 1 && st->prev_channels == 2 && st->silk_mode.toMono == 0
        && st->mode != MODE_CELT_ONLY && st->prev_mode != MODE_CELT_ONLY)
    {
        /* Delay stereo->mono transition by two frames so that SILK can do a smooth downmix */
        st->silk_mode.toMono = 1;
        st->stream_channels = 2;
    }
    else {
        st->silk_mode.toMono = 0;
    }

    /* Update equivalent rate with mode decision. */
    equiv_rate = compute_equiv_rate(st->bitrate_bps, st->stream_channels, st->Fs / frame_size,
        st->use_vbr, st->mode, st->silk_mode.complexity, st->silk_mode.packetLossPercentage);

    if (st->mode != MODE_CELT_ONLY && st->prev_mode == MODE_CELT_ONLY)
    {
        silk_EncControlStruct dummy;
        silk_InitEncoder(silk_enc, st->arch, &dummy);
        prefill = 1;
    }

    /* Automatic (rate-dependent) bandwidth selection */
    if (st->mode == MODE_CELT_ONLY || st->first || st->silk_mode.allowBandwidthSwitch)
    {
        const opus_int32* voice_bandwidth_thresholds, * music_bandwidth_thresholds;
        opus_int32 bandwidth_thresholds[8];
        int bandwidth = OPUS_BANDWIDTH_FULLBAND;

        if (st->channels == 2 && st->force_channels != 1)
        {
            voice_bandwidth_thresholds = stereo_voice_bandwidth_thresholds;
            music_bandwidth_thresholds = stereo_music_bandwidth_thresholds;
        }
        else {
            voice_bandwidth_thresholds = mono_voice_bandwidth_thresholds;
            music_bandwidth_thresholds = mono_music_bandwidth_thresholds;
        }
        /* Interpolate bandwidth thresholds depending on voice estimation */
        for (i = 0; i < 8; i++)
        {
            bandwidth_thresholds[i] = music_bandwidth_thresholds[i]
                + ((voice_est * voice_est * (voice_bandwidth_thresholds[i] - music_bandwidth_thresholds[i])) >> 14);
        }
        do {
            int threshold, hysteresis;
            threshold = bandwidth_thresholds[2 * (bandwidth - OPUS_BANDWIDTH_MEDIUMBAND)];
            hysteresis = bandwidth_thresholds[2 * (bandwidth - OPUS_BANDWIDTH_MEDIUMBAND) + 1];
            if (!st->first)
            {
                if (st->auto_bandwidth >= bandwidth)
                    threshold -= hysteresis;
                else
                    threshold += hysteresis;
            }
            if (equiv_rate >= threshold)
                break;
        } while (--bandwidth > OPUS_BANDWIDTH_NARROWBAND);
        /* We don't use mediumband anymore, except when explicitly requested or during
           mode transitions. */
        if (bandwidth == OPUS_BANDWIDTH_MEDIUMBAND)
            bandwidth = OPUS_BANDWIDTH_WIDEBAND;
        st->bandwidth = st->auto_bandwidth = bandwidth;
        /* Prevents any transition to SWB/FB until the SILK layer has fully
           switched to WB mode and turned the variable LP filter off */
        if (!st->first && st->mode != MODE_CELT_ONLY && !st->silk_mode.inWBmodeWithoutVariableLP && st->bandwidth > OPUS_BANDWIDTH_WIDEBAND)
            st->bandwidth = OPUS_BANDWIDTH_WIDEBAND;
    }

    if (st->bandwidth > st->max_bandwidth)
        st->bandwidth = st->max_bandwidth;

    if (st->user_bandwidth != OPUS_AUTO)
        st->bandwidth = st->user_bandwidth;

    /* This prevents us from using hybrid at unsafe CBR/max rates */
    if (st->mode != MODE_CELT_ONLY && max_rate < 15000)
    {
        st->bandwidth = IMIN(st->bandwidth, OPUS_BANDWIDTH_WIDEBAND);
    }

    /* Prevents Opus from wasting bits on frequencies that are above
       the Nyquist rate of the input signal */
    if (st->Fs <= 24000 && st->bandwidth > OPUS_BANDWIDTH_SUPERWIDEBAND)
        st->bandwidth = OPUS_BANDWIDTH_SUPERWIDEBAND;
    if (st->Fs <= 16000 && st->bandwidth > OPUS_BANDWIDTH_WIDEBAND)
        st->bandwidth = OPUS_BANDWIDTH_WIDEBAND;
    if (st->Fs <= 12000 && st->bandwidth > OPUS_BANDWIDTH_MEDIUMBAND)
        st->bandwidth = OPUS_BANDWIDTH_MEDIUMBAND;
    if (st->Fs <= 8000 && st->bandwidth > OPUS_BANDWIDTH_NARROWBAND)
        st->bandwidth = OPUS_BANDWIDTH_NARROWBAND;
#ifndef DISABLE_FLOAT_API
    /* Use detected bandwidth to reduce the encoded bandwidth. */
    if (st->detected_bandwidth && st->user_bandwidth == OPUS_AUTO)
    {
        int min_detected_bandwidth;
        /* Makes bandwidth detection more conservative just in case the detector
           gets it wrong when we could have coded a high bandwidth transparently.
           When operating in SILK/hybrid mode, we don't go below wideband to avoid
           more complicated switches that require redundancy. */
        if (equiv_rate <= 18000 * st->stream_channels && st->mode == MODE_CELT_ONLY)
            min_detected_bandwidth = OPUS_BANDWIDTH_NARROWBAND;
        else if (equiv_rate <= 24000 * st->stream_channels && st->mode == MODE_CELT_ONLY)
            min_detected_bandwidth = OPUS_BANDWIDTH_MEDIUMBAND;
        else if (equiv_rate <= 30000 * st->stream_channels)
            min_detected_bandwidth = OPUS_BANDWIDTH_WIDEBAND;
        else if (equiv_rate <= 44000 * st->stream_channels)
            min_detected_bandwidth = OPUS_BANDWIDTH_SUPERWIDEBAND;
        else
            min_detected_bandwidth = OPUS_BANDWIDTH_FULLBAND;

        st->detected_bandwidth = IMAX(st->detected_bandwidth, min_detected_bandwidth);
        st->bandwidth = IMIN(st->bandwidth, st->detected_bandwidth);
    }
#endif
    st->silk_mode.LBRR_coded = decide_fec(st->silk_mode.useInBandFEC, st->silk_mode.packetLossPercentage,
        st->silk_mode.LBRR_coded, st->mode, &st->bandwidth, equiv_rate);
    celt_encoder_ctl(celt_enc, OPUS_SET_LSB_DEPTH(lsb_depth));

    /* CELT mode doesn't support mediumband, use wideband instead */
    if (st->mode == MODE_CELT_ONLY && st->bandwidth == OPUS_BANDWIDTH_MEDIUMBAND)
        st->bandwidth = OPUS_BANDWIDTH_WIDEBAND;
    if (st->lfe)
        st->bandwidth = OPUS_BANDWIDTH_NARROWBAND;

    curr_bandwidth = st->bandwidth;

    /* Chooses the appropriate mode for speech
       *NEVER* switch to/from CELT-only mode here as this will invalidate some assumptions */
    if (st->mode == MODE_SILK_ONLY && curr_bandwidth > OPUS_BANDWIDTH_WIDEBAND)
        st->mode = MODE_HYBRID;
    if (st->mode == MODE_HYBRID && curr_bandwidth <= OPUS_BANDWIDTH_WIDEBAND)
        st->mode = MODE_SILK_ONLY;

    /* Can't support higher than >60 ms frames, and >20 ms when in Hybrid or CELT-only modes */
    if ((frame_size > st->Fs / 50 && (st->mode != MODE_SILK_ONLY)) || frame_size > 3 * st->Fs / 50)
    {
        int enc_frame_size;
        int nb_frames;

        if (st->mode == MODE_SILK_ONLY)
        {
            if (frame_size == 2 * st->Fs / 25)  /* 80 ms -> 2x 40 ms */
                enc_frame_size = st->Fs / 25;
            else if (frame_size == 3 * st->Fs / 25)  /* 120 ms -> 2x 60 ms */
                enc_frame_size = 3 * st->Fs / 50;
            else                            /* 100 ms -> 5x 20 ms */
                enc_frame_size = st->Fs / 50;
        }
        else
            enc_frame_size = st->Fs / 50;

        nb_frames = frame_size / enc_frame_size;

#ifndef DISABLE_FLOAT_API
        if (analysis_read_pos_bak != -1)
        {
            st->analysis.read_pos = analysis_read_pos_bak;
            st->analysis.read_subframe = analysis_read_subframe_bak;
        }
#endif

        ret = encode_multiframe_packet(st, pcm, nb_frames, enc_frame_size, data,
            out_data_bytes, to_celt, lsb_depth, float_api);

        RESTORE_STACK;
        return ret;
    }

    /* For the first frame at a new SILK bandwidth */
    if (st->silk_bw_switch)
    {
        redundancy = 1;
        celt_to_silk = 1;
        st->silk_bw_switch = 0;
        /* Do a prefill without reseting the sampling rate control. */
        prefill = 2;
    }

    /* If we decided to go with CELT, make sure redundancy is off, no matter what
       we decided earlier. */
    if (st->mode == MODE_CELT_ONLY)
        redundancy = 0;

    if (redundancy)
    {
        redundancy_bytes = compute_redundancy_bytes(max_data_bytes, st->bitrate_bps, frame_rate, st->stream_channels);
        if (redundancy_bytes == 0)
            redundancy = 0;
    }

    /* printf("%d %d %d %d\n", st->bitrate_bps, st->stream_channels, st->mode, curr_bandwidth); */
    bytes_target = IMIN(max_data_bytes - redundancy_bytes, st->bitrate_bps * frame_size / (st->Fs * 8)) - 1;

    data += 1;

    ec_enc_init(&enc, data, max_data_bytes - 1);

    ALLOC(pcm_buf, (total_buffer + frame_size) * st->channels, opus_val16);
    OPUS_COPY(pcm_buf, &st->delay_buffer[(st->encoder_buffer - total_buffer) * st->channels], total_buffer * st->channels);

    if (st->mode == MODE_CELT_ONLY)
        hp_freq_smth1 = silk_LSHIFT(silk_lin2log(VARIABLE_HP_MIN_CUTOFF_HZ), 8);
    else
        hp_freq_smth1 = ((silk_encoder*)silk_enc)->state_Fxx[0].sCmn.variable_HP_smth1_Q15;

    st->variable_HP_smth2_Q15 = silk_SMLAWB(st->variable_HP_smth2_Q15,
        hp_freq_smth1 - st->variable_HP_smth2_Q15, SILK_FIX_CONST(VARIABLE_HP_SMTH_COEF2, 16));

    /* convert from log scale to Hertz */
    cutoff_Hz = silk_log2lin(silk_RSHIFT(st->variable_HP_smth2_Q15, 8));

    if (st->application == OPUS_APPLICATION_VOIP)
    {
        hp_cutoff(pcm, cutoff_Hz, &pcm_buf[total_buffer * st->channels], st->hp_mem, frame_size, st->channels, st->Fs, st->arch);
    }
    else {
        dc_reject(pcm, 3, &pcm_buf[total_buffer * st->channels], st->hp_mem, frame_size, st->channels, st->Fs);
    }




    /* SILK processing */
    HB_gain = Q15ONE;
    if (st->mode != MODE_CELT_ONLY)
    {
        opus_int32 total_bitRate, celt_rate;
#ifdef FIXED_POINT
        const opus_int16* pcm_silk;
#else
        VARDECL(opus_int16, pcm_silk);
        ALLOC(pcm_silk, st->channels * frame_size, opus_int16);
#endif

        /* Distribute bits between SILK and CELT */
        total_bitRate = 8 * bytes_target * frame_rate;
        if (st->mode == MODE_HYBRID) {
            /* Base rate for SILK */
            st->silk_mode.bitRate = compute_silk_rate_for_hybrid(total_bitRate,
                curr_bandwidth, st->Fs == 50 * frame_size, st->use_vbr, st->silk_mode.LBRR_coded,
                st->stream_channels);
            if (!st->energy_masking)
            {
                /* Increasingly attenuate high band when it gets allocated fewer bits */
                celt_rate = total_bitRate - st->silk_mode.bitRate;
                HB_gain = Q15ONE - SHR32(celt_exp2(-celt_rate * QCONST16(1.f / 1024, 10)), 1);
            }
        }
        else {
            /* SILK gets all bits */
            st->silk_mode.bitRate = total_bitRate;
        }

        /* Surround masking for SILK */
        if (st->energy_masking && st->use_vbr && !st->lfe)
        {
            opus_val32 mask_sum = 0;
            opus_val16 masking_depth;
            opus_int32 rate_offset;
            int c;
            int end = 17;
            opus_int16 srate = 16000;
            if (st->bandwidth == OPUS_BANDWIDTH_NARROWBAND)
            {
                end = 13;
                srate = 8000;
            }
            else if (st->bandwidth == OPUS_BANDWIDTH_MEDIUMBAND)
            {
                end = 15;
                srate = 12000;
            }
            for (c = 0; c < st->channels; c++)
            {
                for (i = 0; i < end; i++)
                {
                    opus_val16 mask;
                    mask = MAX16(MIN16(st->energy_masking[21 * c + i],
                        QCONST16(.5f, DB_SHIFT)), -QCONST16(2.0f, DB_SHIFT));
                    if (mask > 0)
                        mask = HALF16(mask);
                    mask_sum += mask;
                }
            }
            /* Conservative rate reduction, we cut the masking in half */
            masking_depth = mask_sum / end * st->channels;
            masking_depth += QCONST16(.2f, DB_SHIFT);
            rate_offset = (opus_int32)PSHR32(MULT16_16(srate, masking_depth), DB_SHIFT);
            rate_offset = MAX32(rate_offset, -2 * st->silk_mode.bitRate / 3);
            /* Split the rate change between the SILK and CELT part for hybrid. */
            if (st->bandwidth == OPUS_BANDWIDTH_SUPERWIDEBAND || st->bandwidth == OPUS_BANDWIDTH_FULLBAND)
                st->silk_mode.bitRate += 3 * rate_offset / 5;
            else
                st->silk_mode.bitRate += rate_offset;
        }

        st->silk_mode.payloadSize_ms = 1000 * frame_size / st->Fs;
        st->silk_mode.nChannelsAPI = st->channels;
        st->silk_mode.nChannelsInternal = st->stream_channels;
        if (curr_bandwidth == OPUS_BANDWIDTH_NARROWBAND) {
            st->silk_mode.desiredInternalSampleRate = 8000;
        }
        else if (curr_bandwidth == OPUS_BANDWIDTH_MEDIUMBAND) {
            st->silk_mode.desiredInternalSampleRate = 12000;
        }
        else {
            celt_assert(st->mode == MODE_HYBRID || curr_bandwidth == OPUS_BANDWIDTH_WIDEBAND);
            st->silk_mode.desiredInternalSampleRate = 16000;
        }
        if (st->mode == MODE_HYBRID) {
            /* Don't allow bandwidth reduction at lowest bitrates in hybrid mode */
            st->silk_mode.minInternalSampleRate = 16000;
        }
        else {
            st->silk_mode.minInternalSampleRate = 8000;
        }

        st->silk_mode.maxInternalSampleRate = 16000;
        if (st->mode == MODE_SILK_ONLY)
        {
            opus_int32 effective_max_rate = max_rate;
            if (frame_rate > 50)
                effective_max_rate = effective_max_rate * 2 / 3;
            if (effective_max_rate < 8000)
            {
                st->silk_mode.maxInternalSampleRate = 12000;
                st->silk_mode.desiredInternalSampleRate = IMIN(12000, st->silk_mode.desiredInternalSampleRate);
            }
            if (effective_max_rate < 7000)
            {
                st->silk_mode.maxInternalSampleRate = 8000;
                st->silk_mode.desiredInternalSampleRate = IMIN(8000, st->silk_mode.desiredInternalSampleRate);
            }
        }

        st->silk_mode.useCBR = !st->use_vbr;

        /* Call SILK encoder for the low band */

        /* Max bits for SILK, counting ToC, redundancy bytes, and optionally redundancy. */
        st->silk_mode.maxBits = (max_data_bytes - 1) * 8;
        if (redundancy && redundancy_bytes >= 2)
        {
            /* Counting 1 bit for redundancy position and 20 bits for flag+size (only for hybrid). */
            st->silk_mode.maxBits -= redundancy_bytes * 8 + 1;
            if (st->mode == MODE_HYBRID)
                st->silk_mode.maxBits -= 20;
        }
        if (st->silk_mode.useCBR)
        {
            if (st->mode == MODE_HYBRID)
            {
                st->silk_mode.maxBits = IMIN(st->silk_mode.maxBits, st->silk_mode.bitRate * frame_size / st->Fs);
            }
        }
        else {
            /* Constrained VBR. */
            if (st->mode == MODE_HYBRID)
            {
                /* Compute SILK bitrate corresponding to the max total bits available */
                opus_int32 maxBitRate = compute_silk_rate_for_hybrid(st->silk_mode.maxBits * st->Fs / frame_size,
                    curr_bandwidth, st->Fs == 50 * frame_size, st->use_vbr, st->silk_mode.LBRR_coded,
                    st->stream_channels);
                st->silk_mode.maxBits = maxBitRate * frame_size / st->Fs;
            }
        }

        if (prefill)
        {
            opus_int32 zero = 0;
            int prefill_offset;
            /* Use a smooth onset for the SILK prefill to avoid the encoder trying to encode
               a discontinuity. The exact location is what we need to avoid leaving any "gap"
               in the audio when mixing with the redundant CELT frame. Here we can afford to
               overwrite st->delay_buffer because the only thing that uses it before it gets
               rewritten is tmp_prefill[] and even then only the part after the ramp really
               gets used (rather than sent to the encoder and discarded) */
            prefill_offset = st->channels * (st->encoder_buffer - st->delay_compensation - st->Fs / 400);
            gain_fade(st->delay_buffer + prefill_offset, st->delay_buffer + prefill_offset,
                0, Q15ONE, celt_mode->overlap, st->Fs / 400, st->channels, celt_mode->window, st->Fs);
            OPUS_CLEAR(st->delay_buffer, prefill_offset);
#ifdef FIXED_POINT
            pcm_silk = st->delay_buffer;
#else
            for (i = 0; i < st->encoder_buffer * st->channels; i++)
                pcm_silk[i] = FLOAT2INT16(st->delay_buffer[i]);
#endif
            silk_Encode(silk_enc, &st->silk_mode, pcm_silk, st->encoder_buffer, NULL, &zero, prefill, activity);
            /* Prevent a second switch in the real encode call. */
            st->silk_mode.opusCanSwitch = 0;
        }

#ifdef FIXED_POINT
        pcm_silk = pcm_buf + total_buffer * st->channels;
#else
        for (i = 0; i < frame_size * st->channels; i++)
            pcm_silk[i] = FLOAT2INT16(pcm_buf[total_buffer * st->channels + i]);
#endif
        ret = silk_Encode(silk_enc, &st->silk_mode, pcm_silk, frame_size, &enc, &nBytes, 0, activity);
        if (ret) {
            /*fprintf (stderr, "SILK encode error: %d\n", ret);*/
            /* Handle error */
            RESTORE_STACK;
            return OPUS_INTERNAL_ERROR;
        }

        /* Extract SILK internal bandwidth for signaling in first byte */
        if (st->mode == MODE_SILK_ONLY) {
            if (st->silk_mode.internalSampleRate == 8000) {
                curr_bandwidth = OPUS_BANDWIDTH_NARROWBAND;
            }
            else if (st->silk_mode.internalSampleRate == 12000) {
                curr_bandwidth = OPUS_BANDWIDTH_MEDIUMBAND;
            }
            else if (st->silk_mode.internalSampleRate == 16000) {
                curr_bandwidth = OPUS_BANDWIDTH_WIDEBAND;
            }
        }
        else {
            celt_assert(st->silk_mode.internalSampleRate == 16000);
        }

        st->silk_mode.opusCanSwitch = st->silk_mode.switchReady && !st->nonfinal_frame;

        if (nBytes == 0)
        {
            st->rangeFinal = 0;
            data[-1] = gen_toc(st->mode, st->Fs / frame_size, curr_bandwidth, st->stream_channels);
            RESTORE_STACK;
            return 1;
        }

        /* FIXME: How do we allocate the redundancy for CBR? */
        if (st->silk_mode.opusCanSwitch)
        {
            redundancy_bytes = compute_redundancy_bytes(max_data_bytes, st->bitrate_bps, frame_rate, st->stream_channels);
            redundancy = (redundancy_bytes != 0);
            celt_to_silk = 0;
            st->silk_bw_switch = 1;
        }
    }

    /* CELT processing */
    {
        int endband = 21;

        switch (curr_bandwidth)
        {
        case OPUS_BANDWIDTH_NARROWBAND:
            endband = 13;
            break;
        case OPUS_BANDWIDTH_MEDIUMBAND:
        case OPUS_BANDWIDTH_WIDEBAND:
            endband = 17;
            break;
        case OPUS_BANDWIDTH_SUPERWIDEBAND:
            endband = 19;
            break;
        case OPUS_BANDWIDTH_FULLBAND:
            endband = 21;
            break;
        }
        celt_encoder_ctl(celt_enc, CELT_SET_END_BAND(endband));
        celt_encoder_ctl(celt_enc, CELT_SET_CHANNELS(st->stream_channels));
    }
    celt_encoder_ctl(celt_enc, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));
    if (st->mode != MODE_SILK_ONLY)
    {
        opus_val32 celt_pred = 2;
        celt_encoder_ctl(celt_enc, OPUS_SET_VBR(0));
        /* We may still decide to disable prediction later */
        if (st->silk_mode.reducedDependency)
            celt_pred = 0;
        celt_encoder_ctl(celt_enc, CELT_SET_PREDICTION(celt_pred));

        if (st->mode == MODE_HYBRID)
        {
            if (st->use_vbr) {
                celt_encoder_ctl(celt_enc, OPUS_SET_BITRATE(st->bitrate_bps - st->silk_mode.bitRate));
                celt_encoder_ctl(celt_enc, OPUS_SET_VBR_CONSTRAINT(0));
            }
        }
        else {
            if (st->use_vbr)
            {
                celt_encoder_ctl(celt_enc, OPUS_SET_VBR(1));
                celt_encoder_ctl(celt_enc, OPUS_SET_VBR_CONSTRAINT(st->vbr_constraint));
                celt_encoder_ctl(celt_enc, OPUS_SET_BITRATE(st->bitrate_bps));
            }
        }
    }

    ALLOC(tmp_prefill, st->channels * st->Fs / 400, opus_val16);
    if (st->mode != MODE_SILK_ONLY && st->mode != st->prev_mode && st->prev_mode > 0)
    {
        OPUS_COPY(tmp_prefill, &st->delay_buffer[(st->encoder_buffer - total_buffer - st->Fs / 400) * st->channels], st->channels * st->Fs / 400);
    }

    if (st->channels * (st->encoder_buffer - (frame_size + total_buffer)) > 0)
    {
        OPUS_MOVE(st->delay_buffer, &st->delay_buffer[st->channels * frame_size], st->channels * (st->encoder_buffer - frame_size - total_buffer));
        OPUS_COPY(&st->delay_buffer[st->channels * (st->encoder_buffer - frame_size - total_buffer)],
            &pcm_buf[0],
            (frame_size + total_buffer) * st->channels);
    }
    else {
        OPUS_COPY(st->delay_buffer, &pcm_buf[(frame_size + total_buffer - st->encoder_buffer) * st->channels], st->encoder_buffer * st->channels);
    }
    /* gain_fade() and stereo_fade() need to be after the buffer copying
       because we don't want any of this to affect the SILK part */
    if (st->prev_HB_gain < Q15ONE || HB_gain < Q15ONE) {
        gain_fade(pcm_buf, pcm_buf,
            st->prev_HB_gain, HB_gain, celt_mode->overlap, frame_size, st->channels, celt_mode->window, st->Fs);
    }
    st->prev_HB_gain = HB_gain;
    if (st->mode != MODE_HYBRID || st->stream_channels == 1)
    {
        if (equiv_rate > 32000)
            st->silk_mode.stereoWidth_Q14 = 16384;
        else if (equiv_rate < 16000)
            st->silk_mode.stereoWidth_Q14 = 0;
        else
            st->silk_mode.stereoWidth_Q14 = 16384 - 2048 * (opus_int32)(32000 - equiv_rate) / (equiv_rate - 14000);
    }
    if (!st->energy_masking && st->channels == 2) {
        /* Apply stereo width reduction (at low bitrates) */
        if (st->hybrid_stereo_width_Q14 < (1 << 14) || st->silk_mode.stereoWidth_Q14 < (1 << 14)) {
            opus_val16 g1, g2;
            g1 = st->hybrid_stereo_width_Q14;
            g2 = (opus_val16)(st->silk_mode.stereoWidth_Q14);
#ifdef FIXED_POINT
            g1 = g1 == 16384 ? Q15ONE : SHL16(g1, 1);
            g2 = g2 == 16384 ? Q15ONE : SHL16(g2, 1);
#else
            g1 *= (1.f / 16384);
            g2 *= (1.f / 16384);
#endif
            stereo_fade(pcm_buf, pcm_buf, g1, g2, celt_mode->overlap,
                frame_size, st->channels, celt_mode->window, st->Fs);
            st->hybrid_stereo_width_Q14 = st->silk_mode.stereoWidth_Q14;
        }
    }

    if (st->mode != MODE_CELT_ONLY && ec_tell(&enc) + 17 + 20 * (st->mode == MODE_HYBRID) <= 8 * (max_data_bytes - 1))
    {
        /* For SILK mode, the redundancy is inferred from the length */
        if (st->mode == MODE_HYBRID)
            ec_enc_bit_logp(&enc, redundancy, 12);
        if (redundancy)
        {
            int max_redundancy;
            ec_enc_bit_logp(&enc, celt_to_silk, 1);
            if (st->mode == MODE_HYBRID)
            {
                /* Reserve the 8 bits needed for the redundancy length,
                   and at least a few bits for CELT if possible */
                max_redundancy = (max_data_bytes - 1) - ((ec_tell(&enc) + 8 + 3 + 7) >> 3);
            }
            else
                max_redundancy = (max_data_bytes - 1) - ((ec_tell(&enc) + 7) >> 3);
            /* Target the same bit-rate for redundancy as for the rest,
               up to a max of 257 bytes */
            redundancy_bytes = IMIN(max_redundancy, redundancy_bytes);
            redundancy_bytes = IMIN(257, IMAX(2, redundancy_bytes));
            if (st->mode == MODE_HYBRID)
                ec_enc_uint(&enc, redundancy_bytes - 2, 256);
        }
    }
    else {
        redundancy = 0;
    }

    if (!redundancy)
    {
        st->silk_bw_switch = 0;
        redundancy_bytes = 0;
    }
    if (st->mode != MODE_CELT_ONLY)start_band = 17;

    if (st->mode == MODE_SILK_ONLY)
    {
        ret = (ec_tell(&enc) + 7) >> 3;
        ec_enc_done(&enc);
        nb_compr_bytes = ret;
    }
    else {
        nb_compr_bytes = (max_data_bytes - 1) - redundancy_bytes;
        ec_enc_shrink(&enc, nb_compr_bytes);
    }

#ifndef DISABLE_FLOAT_API
    if (redundancy || st->mode != MODE_SILK_ONLY)
        celt_encoder_ctl(celt_enc, CELT_SET_ANALYSIS(&analysis_info));
#endif
    if (st->mode == MODE_HYBRID) {
        SILKInfo info;
        info.signalType = st->silk_mode.signalType;
        info.offset = st->silk_mode.offset;
        celt_encoder_ctl(celt_enc, CELT_SET_SILK_INFO(&info));
    }

    /* 5 ms redundant frame for CELT->SILK */
    if (redundancy && celt_to_silk)
    {
        int err;
        celt_encoder_ctl(celt_enc, CELT_SET_START_BAND(0));
        celt_encoder_ctl(celt_enc, OPUS_SET_VBR(0));
        celt_encoder_ctl(celt_enc, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));
        err = celt_encode_with_ec(celt_enc, pcm_buf, st->Fs / 200, data + nb_compr_bytes, redundancy_bytes, NULL);
        if (err < 0)
        {
            RESTORE_STACK;
            return OPUS_INTERNAL_ERROR;
        }
        celt_encoder_ctl(celt_enc, OPUS_GET_FINAL_RANGE(&redundant_rng));
        celt_encoder_ctl(celt_enc, OPUS_RESET_STATE);
    }

    celt_encoder_ctl(celt_enc, CELT_SET_START_BAND(start_band));

    if (st->mode != MODE_SILK_ONLY)
    {
        if (st->mode != st->prev_mode && st->prev_mode > 0)
        {
            unsigned char dummy[2];
            celt_encoder_ctl(celt_enc, OPUS_RESET_STATE);

            /* Prefilling */
            celt_encode_with_ec(celt_enc, tmp_prefill, st->Fs / 400, dummy, 2, NULL);
            celt_encoder_ctl(celt_enc, CELT_SET_PREDICTION(0));
        }
        /* If false, we already busted the budget and we'll end up with a "PLC frame" */
        if (ec_tell(&enc) <= 8 * nb_compr_bytes)
        {
            /* Set the bitrate again if it was overridden in the redundancy code above*/
            if (redundancy && celt_to_silk && st->mode == MODE_HYBRID && st->use_vbr)
                celt_encoder_ctl(celt_enc, OPUS_SET_BITRATE(st->bitrate_bps - st->silk_mode.bitRate));
            celt_encoder_ctl(celt_enc, OPUS_SET_VBR(st->use_vbr));
            ret = celt_encode_with_ec(celt_enc, pcm_buf, frame_size, NULL, nb_compr_bytes, &enc);
            if (ret < 0)
            {
                RESTORE_STACK;
                return OPUS_INTERNAL_ERROR;
            }
            /* Put CELT->SILK redundancy data in the right place. */
            if (redundancy && celt_to_silk && st->mode == MODE_HYBRID && st->use_vbr)
            {
                OPUS_MOVE(data + ret, data + nb_compr_bytes, redundancy_bytes);
                nb_compr_bytes = nb_compr_bytes + redundancy_bytes;
            }
        }
    }

    /* 5 ms redundant frame for SILK->CELT */
    if (redundancy && !celt_to_silk)
    {
        int err;
        unsigned char dummy[2];
        int N2, N4;
        N2 = st->Fs / 200;
        N4 = st->Fs / 400;

        celt_encoder_ctl(celt_enc, OPUS_RESET_STATE);
        celt_encoder_ctl(celt_enc, CELT_SET_START_BAND(0));
        celt_encoder_ctl(celt_enc, CELT_SET_PREDICTION(0));
        celt_encoder_ctl(celt_enc, OPUS_SET_VBR(0));
        celt_encoder_ctl(celt_enc, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));

        if (st->mode == MODE_HYBRID)
        {
            /* Shrink packet to what the encoder actually used. */
            nb_compr_bytes = ret;
            ec_enc_shrink(&enc, nb_compr_bytes);
        }
        /* NOTE: We could speed this up slightly (at the expense of code size) by just adding a function that prefills the buffer */
        celt_encode_with_ec(celt_enc, pcm_buf + st->channels * (frame_size - N2 - N4), N4, dummy, 2, NULL);

        err = celt_encode_with_ec(celt_enc, pcm_buf + st->channels * (frame_size - N2), N2, data + nb_compr_bytes, redundancy_bytes, NULL);
        if (err < 0)
        {
            RESTORE_STACK;
            return OPUS_INTERNAL_ERROR;
        }
        celt_encoder_ctl(celt_enc, OPUS_GET_FINAL_RANGE(&redundant_rng));
    }



    /* Signalling the mode in the first byte */
    data--;
    data[0] = gen_toc(st->mode, st->Fs / frame_size, curr_bandwidth, st->stream_channels);

    st->rangeFinal = enc.rng ^ redundant_rng;

    if (to_celt)
        st->prev_mode = MODE_CELT_ONLY;
    else
        st->prev_mode = st->mode;
    st->prev_channels = st->stream_channels;
    st->prev_framesize = frame_size;

    st->first = 0;

    /* DTX decision */
#ifndef DISABLE_FLOAT_API
    if (st->use_dtx && (analysis_info.valid || is_silence))
    {
        if (decide_dtx_mode(activity, &st->nb_no_activity_ms_Q1, 2 * 1000 * frame_size / st->Fs))
        {
            st->rangeFinal = 0;
            data[0] = gen_toc(st->mode, st->Fs / frame_size, curr_bandwidth, st->stream_channels);
            RESTORE_STACK;
            return 1;
        }
    }
    else {
        st->nb_no_activity_ms_Q1 = 0;
    }
#endif

    /* In the unlikely case that the SILK encoder busted its target, tell
       the decoder to call the PLC */
    if (ec_tell(&enc) > (max_data_bytes - 1) * 8)
    {
        if (max_data_bytes < 2)
        {
            RESTORE_STACK;
            return OPUS_BUFFER_TOO_SMALL;
        }
        data[1] = 0;
        ret = 1;
        st->rangeFinal = 0;
    }
    else if (st->mode == MODE_SILK_ONLY && !redundancy)
    {
        /*When in LPC only mode it's perfectly
          reasonable to strip off trailing zero bytes as
          the required range decoder behavior is to
          fill these in. This can't be done when the MDCT
          modes are used because the decoder needs to know
          the actual length for allocation purposes.*/
        while (ret > 2 && data[ret] == 0)ret--;
    }
    /* Count ToC and redundancy */
    ret += 1 + redundancy_bytes;
    if (!st->use_vbr)
    {
        if (opus_packet_pad(data, ret, max_data_bytes) != OPUS_OK)
        {
            RESTORE_STACK;
            return OPUS_INTERNAL_ERROR;
        }
        ret = max_data_bytes;
    }
    RESTORE_STACK;
    return ret;
}


opus_int32 opus_encode(OpusEncoder* st, const opus_int16* pcm, int analysis_frame_size,
    unsigned char* data, opus_int32 max_data_bytes)
{
    int i, ret;
    int frame_size;
    VARDECL(float, in);
    ALLOC_STACK;

    frame_size = frame_size_select(analysis_frame_size, st->variable_duration, st->Fs);
    if (frame_size <= 0)
    {
        RESTORE_STACK;
        return OPUS_BAD_ARG;
    }
    ALLOC(in, frame_size * st->channels, float);

    for (i = 0; i < frame_size * st->channels; i++)
        in[i] = (1.0f / 32768) * pcm[i];
    ret = opus_encode_native(st, in, frame_size, data, max_data_bytes, 16,
        pcm, analysis_frame_size, 0, -2, st->channels, downmix_int, 0);
    RESTORE_STACK;
    return ret;
}


int opus_encoder_ctl(OpusEncoder* st, int request, ...)
{
    return 0;
}

void opus_encoder_destroy(OpusEncoder* st)
{
    opus_free(st);
}
