#ifndef BBBOOST_DSP_H
#define BBBOOST_DSP_H

/*
 * BB Boost core. Pure gain only: no clipping, detector, filter or state.
 * The maximum gain is 18 dB (7.943282x), shaped quadratically for finer
 * adjustment near the bottom of the control.
 */

#ifdef __TI_COMPILER_VERSION__
#pragma FUNC_ALWAYS_INLINE(bbboost_process)
#endif
static inline void bbboost_process(float *buffer, float gain_norm, float level_norm)
{
    float gain = 1.0f + 6.943282f * gain_norm * gain_norm;
    float scale = gain * level_norm;
    int i;

    for (i = 0; i < 16; ++i) {
        buffer[i] *= scale;
    }
}

#endif
