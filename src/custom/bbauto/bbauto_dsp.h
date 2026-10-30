#ifndef BBAUTO_DSP_H
#define BBAUTO_DSP_H

#include <stdint.h>

/*
 * Lightweight stereo-linked downward leveler.
 *
 * This is not an RMS loudness meter or a look-ahead compressor. It follows
 * the stereo peak envelope and applies a smoothed gain reduction only above
 * a fixed threshold. One envelope and one gain are shared by L/R.
 */
#define BBAUTO_MAGIC   0x42424155u /* 'BBAU' */
#define BBAUTO_VERSION 1u

#define BBAUTO_THRESHOLD       0.24f
#define BBAUTO_INV_HEADROOM    1.315789474f /* 1 / (1 - threshold) */
#define BBAUTO_MAX_REDUCTION   0.80f
#define BBAUTO_ENV_ATTACK      0.025f
#define BBAUTO_ENV_RELEASE     0.00020f
#define BBAUTO_GAIN_ATTACK     0.020f
#define BBAUTO_GAIN_RELEASE    0.00008f

typedef struct BBAutoState {
    uint32_t magic;
    uint32_t version;
    float envelope;
    float gain;
} BBAutoState;

#ifdef __TI_COMPILER_VERSION__
#pragma FUNC_ALWAYS_INLINE(bbauto_abs)
#endif
static inline float bbauto_abs(float x)
{
    return x < 0.0f ? -x : x;
}

#ifdef __TI_COMPILER_VERSION__
#pragma FUNC_ALWAYS_INLINE(bbauto_process)
#endif
static inline void bbauto_process(float *buffer, BBAutoState *state,
                                  float mode, float amount)
{
    float envelope;
    float gain;
    float maxReduction;
    int i;

    /* AMP mode is a true pass-through. */
    if (mode < 0.5f || amount <= 0.0f) return;

    envelope = state->envelope;
    gain = state->gain;
    maxReduction = BBAUTO_MAX_REDUCTION * amount;

    for (i = 0; i < 8; ++i) {
        float left = buffer[i];
        float right = buffer[i + 8];
        float absL = bbauto_abs(left);
        float absR = bbauto_abs(right);
        float peak = absL > absR ? absL : absR;
        float envCoeff = peak > envelope ? BBAUTO_ENV_ATTACK : BBAUTO_ENV_RELEASE;
        float excess;
        float target;
        float minGain;
        float gainCoeff;

        envelope += envCoeff * (peak - envelope);

        excess = envelope - BBAUTO_THRESHOLD;
        if (excess < 0.0f) excess = 0.0f;

        target = 1.0f - maxReduction * excess * BBAUTO_INV_HEADROOM;
        minGain = 1.0f - maxReduction;
        if (target < minGain) target = minGain;
        if (target > 1.0f) target = 1.0f;

        gainCoeff = target < gain ? BBAUTO_GAIN_ATTACK : BBAUTO_GAIN_RELEASE;
        gain += gainCoeff * (target - gain);

        buffer[i] = left * gain;
        buffer[i + 8] = right * gain;
    }

    state->envelope = envelope;
    state->gain = gain;
}

#endif
