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

#define AL_INPUT_FLOOR 0.0015f
#define AL_ENV_ATTACK  0.0200f
#define AL_ENV_RELEASE 0.0008f
#define AL_GAIN_MIN    0.35f
#define AL_GAIN_MAX    3.00f
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

    /* Target is mean-absolute programme level, not peak level.
     * Range 0.025..0.120; default knob value 45 -> about 0.068. */
    float targetKnob = zoom_param_norm01(params[5], 0.45f);
    float target = 0.025f + targetKnob * 0.095f;

    /* Response controls only the slow gain-riding loop. The envelope detector
     * keeps a fixed fast attack and slower release to avoid transient spikes. */
    float response = zoom_param_norm01(params[6], 0.35f);
    float gainRate = 0.00004f + response * 0.00036f;

    float env = st->envelope;
    float gain = st->gain;
    int i;

    /* ctx[5]: 8 left samples followed by 8 right samples.
     * A linked detector applies identical gain to L/R, preserving stereo image. */
    for (i = 0; i < 8; ++i) {
        float left = fxBuf[i];
        float right = fxBuf[i + 8];
        float detector = 0.5f * (al_abs(left) + al_abs(right));

        if (detector > env)
            env += (detector - env) * AL_ENV_ATTACK;
        else
            env += (detector - env) * AL_ENV_RELEASE;

        if (detector > AL_INPUT_FLOOR) {
            /* Closed-loop level control: drive env*gain toward target.
             * This avoids a runtime divide or reciprocal lookup. */
            float error = target - env * gain;
            gain += error * gainRate;
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
