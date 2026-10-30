/*
 * AutoLvl -- post-amplifier / post-NAM automatic level rider.
 *
 * Place AFTER the amp model or drive effect. A linked-stereo envelope follower
 * estimates the programme level, then a slow feedback controller adjusts one
 * shared gain value. It does not clip, gate, compress peaks or alter tone.
 *
 * CPU design: two envelope comparisons, a few multiplies/adds and clamps per
 * stereo sample. No division, libm, delay line, filters, FFT or oversampling.
 * State is per-instance in the descriptor arena; without a valid arena the
 * effect safely passes audio through.
 */
#include <stdint.h>
#include "../../airwindows/common/zoom_params.h"

#define AUTOLEVEL_AUDIO_FUNC Fx_DLY_AutoLvl
#define AL_MAGIC 0x414C564Cu /* 'ALVL' */
#define AL_VERSION 1u

#define AL_INPUT_FLOOR_POWER 0.00000225f /* (-56.5 dBFS peak)^2 */
#define AL_ENV_COEFF   0.0010f /* ~20 ms power averaging */
#define AL_GAIN_MIN    0.05f
#define AL_GAIN_MAX    4.00f
#define AL_SILENCE_RELAX 0.00010f

typedef struct AutoLevelState {
    uint32_t magic;
    uint32_t version;
    float envelope;
    float gain;
} AutoLevelState;

#define AL_DO_PRAGMA(x) _Pragma(#x)
#define AL_EXPAND_PRAGMA(x) AL_DO_PRAGMA(x)
#define AL_CODE_SECTION(f) AL_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))
AL_CODE_SECTION(AUTOLEVEL_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

static inline float al_abs(float x)
{
    return x < 0.0f ? -x : x;
}

void AUTOLEVEL_AUDIO_FUNC(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    /* Required Zoom magic shuttle. */
    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *,
        *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    /* Bypass must be a transparent pass-through. */
    if (params[0] < 0.5f) return;

    /* Per-instance state: 16 bytes, no audio buffer allocation. */
    volatile unsigned int *desc =
        ZDL_PTR(volatile unsigned int *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end  = (uintptr_t)desc[1];
    if (base == 0u || end <= base || (base & 3u) != 0u) return;

    uintptr_t stateAddr = (base + 7u) & ~(uintptr_t)7u;
    if (stateAddr + sizeof(AutoLevelState) > end) return;
    AutoLevelState *st = (AutoLevelState *)stateAddr;

    if (st->magic != AL_MAGIC || st->version != AL_VERSION) {
        st->magic = AL_MAGIC;
        st->version = AL_VERSION;
        st->envelope = 0.0f;
        st->gain = 1.0f;
    }

    /* Target is output RMS level. The detector stores mean-square power, so
     * compare against target^2 and avoid a costly square root.
     * Range 0.040..0.160; default knob value 45 -> about 0.094. */
    float targetKnob = zoom_param_norm01(params[5], 0.45f);
    float target = 0.040f + targetKnob * 0.120f;
    float targetPower = target * target;

    /* Response controls the feedback-loop speed. This multiplicative update
     * converges without division and has a similar time constant at low and
     * high input levels. */
    float response = zoom_param_norm01(params[6], 0.35f);
    float gainAdapt = 0.002f + response * 0.018f;

    float env = st->envelope;
    float gain = st->gain;
    int i;

    /* ctx[5]: 8 left samples followed by 8 right samples.
     * A linked detector applies identical gain to L/R, preserving stereo image. */
    for (i = 0; i < 8; ++i) {
        float left = fxBuf[i];
        float right = fxBuf[i + 8];
        float detectorPower = 0.5f * (left * left + right * right);

        /* One-pole mean-square detector. The same coefficient on attack and
         * release gives a stable RMS estimate without sqrtf(). */
        env += (detectorPower - env) * AL_ENV_COEFF;

        if (detectorPower > AL_INPUT_FLOOR_POWER) {
            /* Closed-loop RMS control: drive env*gain^2 toward target^2.
             * No runtime division, reciprocal, or square root. */
            float error = targetPower - env * gain * gain;
            gain += gain * error * gainAdapt;
        } else {
            /* Do not wind gain up during silence; drift gently back to unity. */
            gain += (1.0f - gain) * AL_SILENCE_RELAX;
        }

        if (gain < AL_GAIN_MIN) gain = AL_GAIN_MIN;
        else if (gain > AL_GAIN_MAX) gain = AL_GAIN_MAX;

        fxBuf[i] = left * gain;
        fxBuf[i + 8] = right * gain;
    }

    st->envelope = env;
    st->gain = gain;
}
