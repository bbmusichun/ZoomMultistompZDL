/*
 * BB Delay — custom single-repeat delay for Zoom G1Xon development.
 *
 * Signal flow:
 *   PRE : booster -> CLEAN/TAPE -> 3-band EQ -> delay -> dry/wet mix
 *   POST: CLEAN/TAPE -> 3-band EQ -> delay -> dry/wet mix -> booster
 *
 * v1 deliberately has no feedback control and no feedback loop: the recorded
 * buffer receives the processed input once, producing one repeat.
 *
 * Safety:
 *   - Delay samples live in the ctx[3] arena, never in static storage.
 *   - Capacity is selected from arena bounds at runtime (8k/16k/32k samples
 *     per channel). If the smallest stereo ring does not fit, DSP returns dry.
 *   - The ring is zeroed incrementally in 256-sample chunks.
 *   - No switch/jump table, heap, static arrays, modulo, runtime math library,
 *     or float-to-unsigned conversion.
 *   - ctx[11]/ctx[12] magic shuttle is preserved.
 *
 * IMPORTANT: G1Xon ctx[3] semantics and capacity are not hardware-confirmed.
 * DSP build is opt-in; build.py defaults to audio_nop=True.
 */

#include <stdint.h>
#include "../../airwindows/common/zoom_params.h"

#define BB_DO_PRAGMA(x) _Pragma(#x)
#define BB_EXPAND_PRAGMA(x) BB_DO_PRAGMA(x)
#define BB_CODE_SECTION(f) BB_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))
#define BB_ALWAYS_INLINE(f) BB_DO_PRAGMA(FUNC_ALWAYS_INLINE(f))

#define BB_CODE_FUNC Fx_DLY_BBDelay
BB_CODE_SECTION(BB_CODE_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define BB_MAGIC       0x4242444Cu
#define BB_VERSION     1u
#define BB_MIN_CAP     8192u
#define BB_MAX_CAP     32768u
#define BB_CLEAR_STEP  256u
#define BB_MIN_DELAY   882.0f
#define BB_DENORM      1.0e-18f

typedef struct BBDelayState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t clearIndex;
    uint32_t capacity;
    uint32_t writePos;
    float lowL;
    float lowR;
    float highL;
    float highR;
} BBDelayState;

BB_ALWAYS_INLINE(bb_clamp)
static inline float bb_clamp(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

BB_ALWAYS_INLINE(bb_flush)
static inline float bb_flush(float x)
{
    if (x < BB_DENORM && x > -BB_DENORM) return 0.0f;
    return x;
}

/* Exponential gain curve approximating 10^(dB/20) for 0..12 dB.
 * Fifth-order Taylor avoids powf and runtime helper calls. */
BB_ALWAYS_INLINE(bb_boost_gain)
static inline float bb_boost_gain(float amount)
{
    float x = amount * 1.38155106f;
    float x2 = x * x;
    float x3 = x2 * x;
    float x4 = x3 * x;
    float x5 = x4 * x;
    return 1.0f + x + 0.5f * x2 + 0.16666667f * x3
         + 0.04166667f * x4 + 0.0083333333f * x5;
}

/* Approximately +/-6 dB around the centre detent. */
BB_ALWAYS_INLINE(bb_eq_gain)
static inline float bb_eq_gain(float n)
{
    if (n < 0.5f) return 1.0f - (0.5f - n);
    return 1.0f + (n - 0.5f) * 2.0f;
}

/* Gentle tape-like soft saturation, active only in TAPE mode. */
BB_ALWAYS_INLINE(bb_tape)
static inline float bb_tape(float x)
{
    x = bb_clamp(x, -2.4f, 2.4f);
    return x - (x * x * x) * 0.05f;
}

void BB_CODE_FUNC(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    /* Preserve the Zoom DSP context shuttle exactly as existing effects do. */
    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *, *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    volatile unsigned int *desc = ZDL_PTR(volatile unsigned int *, ctx[3]);
    if (!desc) return;

    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end  = (uintptr_t)desc[1];
    uint32_t span  = desc[2];

    if (base == 0u || end <= base) return;
    if ((base & 3u) != 0u || (end & 3u) != 0u || (span & 3u) != 0u) return;

    uintptr_t bytes = end - base;
    if ((uintptr_t)span < bytes) return;
    if (bytes < sizeof(BBDelayState) + 65536u) return;

    uintptr_t bufBase = (base + sizeof(BBDelayState) + 7u) & ~(uintptr_t)7u;
    if (bufBase >= end) return;

    uint32_t availableFloats = (uint32_t)((end - bufBase) >> 2);
    uint32_t perChannel = availableFloats >> 1;
    uint32_t capacity = 0u;
    if (perChannel >= 8192u) capacity = 8192u;
    if (perChannel >= 16384u) capacity = 16384u;
    if (perChannel >= 32768u) capacity = 32768u;
    if (capacity < BB_MIN_CAP) return;
    if (capacity > BB_MAX_CAP) capacity = BB_MAX_CAP;
    if (bufBase + (uintptr_t)capacity * 8u > end) return;

    BBDelayState *st = (BBDelayState *)base;
    float *delayL = (float *)bufBase;
    float *delayR = delayL + capacity;
    uint32_t mask = capacity - 1u;

    if (st->magic != BB_MAGIC || st->version != BB_VERSION ||
        st->capacity != capacity) {
        st->magic = BB_MAGIC;
        st->version = BB_VERSION;
        st->initialized = 0u;
        st->clearIndex = 0u;
        st->capacity = capacity;
        st->writePos = 0u;
        st->lowL = st->lowR = 0.0f;
        st->highL = st->highR = 0.0f;
        return;
    }

    if (!st->initialized) {
        uint32_t start = st->clearIndex;
        uint32_t finish = start + BB_CLEAR_STEP;
        if (finish > capacity) finish = capacity;
        uint32_t j;
        for (j = start; j < finish; j++) {
            delayL[j] = 0.0f;
            delayR[j] = 0.0f;
        }
        st->clearIndex = finish;
        if (finish >= capacity) st->initialized = 1u;
        return;
    }

    float charN  = zoom_param_norm01(params[5], 0.0f);
    float boostN = zoom_param_norm01(params[6], 0.0f);
    float posN   = zoom_param_norm01(params[7], 0.0f);
    float levelN = zoom_param_norm01(params[8], 0.0f);
    float bassN  = zoom_param_norm01(params[9], 0.5f);
    float midN   = zoom_param_norm01(params[10], 0.5f);
    float trebN  = zoom_param_norm01(params[11], 0.5f);
    float timeN  = zoom_param_norm01(params[12], 0.5f);
    float mix    = zoom_param_norm01(params[13], 0.3f);

    float boostOn = (boostN >= 0.5f) ? 1.0f : 0.0f;
    float preSel  = (posN < 0.5f) ? 1.0f : 0.0f;
    float tapeOn  = (charN >= 0.5f) ? 1.0f : 0.0f;

    float rawBoost = bb_boost_gain(levelN);
    float preGain  = 1.0f + preSel * boostOn * (rawBoost - 1.0f);
    float postGain = 1.0f + (1.0f - preSel) * boostOn * (rawBoost - 1.0f);

    float gainBass = bb_eq_gain(bassN);
    float gainMid  = bb_eq_gain(midN);
    float gainHigh = bb_eq_gain(trebN);

    float maxDelay = (float)(capacity - 64u);
    float delayF = BB_MIN_DELAY + timeN * (maxDelay - BB_MIN_DELAY);
    int32_t delaySamples = (int32_t)(delayF + 0.5f);
    if (delaySamples < 32) delaySamples = 32;
    if ((uint32_t)delaySamples >= capacity) delaySamples = (int32_t)(capacity - 1u);

    uint32_t wp = st->writePos;
    float lowL = st->lowL, lowR = st->lowR;
    float highL = st->highL, highR = st->highR;
    float dryMix = 1.0f - mix;

    int i;
    for (i = 0; i < 8; i++) {
        float inL = fxBuf[i];
        float inR = fxBuf[i + 8];

        /* Optional PRE booster: before tape colour, EQ and delay record. */
        float srcL = inL * preGain;
        float srcR = inR * preGain;

        float tapeL = bb_tape(srcL);
        float tapeR = bb_tape(srcR);
        srcL += tapeOn * (tapeL - srcL);
        srcR += tapeOn * (tapeR - srcR);

        /* Two one-pole splits form low / mid / high bands without arrays. */
        lowL += 0.035f * (srcL - lowL);
        lowR += 0.035f * (srcR - lowR);
        highL += 0.25f * (srcL - highL);
        highR += 0.25f * (srcR - highR);

        float midBandL = highL - lowL;
        float midBandR = highR - lowR;
        float highBandL = srcL - highL;
        float highBandR = srcR - highR;

        float eqL = lowL * gainBass + midBandL * gainMid + highBandL * gainHigh;
        float eqR = lowR * gainBass + midBandR * gainMid + highBandR * gainHigh;

        uint32_t rp = (wp - (uint32_t)delaySamples) & mask;
        float echoL = delayL[rp];
        float echoR = delayR[rp];

        /* Fixed zero feedback: record only the current processed input. */
        delayL[wp] = eqL;
        delayR[wp] = eqR;
        wp = (wp + 1u) & mask;

        float outL = eqL * dryMix + echoL * mix;
        float outR = eqR * dryMix + echoR * mix;

        /* POST raises the complete dry/wet signal, including the echo. */
        outL *= postGain;
        outR *= postGain;

        fxBuf[i] = bb_flush(outL);
        fxBuf[i + 8] = bb_flush(outR);
    }

    st->writePos = wp;
    st->lowL = bb_flush(lowL);
    st->lowR = bb_flush(lowR);
    st->highL = bb_flush(highL);
    st->highR = bb_flush(highR);
}
