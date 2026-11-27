#include <opus.h>
extern "C" {
#include "celt/celt.h"
}

#include <vector>
#include <algorithm>

struct OpusCustomEncoder {
    const OpusCustomMode* mode;     /**< Mode used by the encoder */
    int channels;
    int stream_channels;

    int force_intra;
    int clip;
    int disable_pf;
    int complexity;
    int upsample;
    int start, end;

    opus_int32 bitrate;
    int vbr;
    int signalling;
    int constrained_vbr;      /* If zero, VBR can do whatever it likes with the rate */
    int loss_rate;
    int lsb_depth;
    int lfe;
    int disable_inv;
    int arch;

    /* Everything beyond this point gets cleared on a reset */
#define ENCODER_RESET_START rng

    opus_uint32 rng;
    int spread_decision;
    opus_val32 delayedIntra;
    int tonal_average;
    int lastCodedBands;
    int hf_average;
    int tapset_decision;

    int prefilter_period;
    opus_val16 prefilter_gain;
    int prefilter_tapset;
#ifdef RESYNTH
    int prefilter_period_old;
    opus_val16 prefilter_gain_old;
    int prefilter_tapset_old;
#endif
    int consec_transient;
    AnalysisInfo analysis;
    SILKInfo silk_info;

    opus_val32 preemph_memE[2];
    opus_val32 preemph_memD[2];

    /* VBR-related parameters */
    opus_int32 vbr_reservoir;
    opus_int32 vbr_drift;
    opus_int32 vbr_offset;
    opus_int32 vbr_count;
    opus_val32 overlap_max;
    opus_val16 stereo_saving;
    int intensity;
    opus_val16* energy_mask;
    opus_val16 spec_avg;

#ifdef RESYNTH
    /* +MAX_PERIOD/2 to make space for overlap */
    celt_sig syn_mem[2][2 * MAX_PERIOD + MAX_PERIOD / 2];
#endif

    celt_sig in_mem[1]; 
};
class AudioEncodeProcessor {
public:
    int encodeOpus(OpusEncoder* encoder, const opus_int16* pcm, int frame_size, unsigned char* data, opus_int32 max_data_bytes) {
        int safe_frame_size = std::max(120, std::min(frame_size, 5760));
        int safe_max_bytes = std::max(64, max_data_bytes);

        return opus_encode(encoder, pcm, safe_frame_size, data, safe_max_bytes);
    }

    int encodeCELT(CELTEncoder* encoder, const opus_int16* pcm, int frame_size, unsigned char* data, int max_data_bytes, ec_enc* ec) {
        int force_frame_size = frame_size; // do not cap
        int force_max_bytes = max_data_bytes; // do not cap

        std::vector<opus_val16> pcm_converted(force_frame_size);
        for (int i = 0; i < force_frame_size; ++i) {
            pcm_converted[i] = static_cast<opus_val16>(pcm[i]);
        }

        if (encoder) {
            encoder->constrained_vbr = 0;
            encoder->bitrate = OPUS_BITRATE_MAX;
            encoder->clip = 0;
            encoder->disable_pf = 1;
            encoder->force_intra = 1;
        }

        return celt_encode_with_ec(encoder, pcm_converted.data(), force_frame_size, data, force_max_bytes, ec);
    }
};

// Global instance
static AudioEncodeProcessor g_processor;

// Export function for Main.cpp to use
extern "C" int opus_encode_with_processing(OpusEncoder* st, const opus_int16* pcm, int frame_size, unsigned char* data, opus_int32 max_data_bytes)
{
    return g_processor.encodeOpus(st, pcm, frame_size, data, max_data_bytes);
}
