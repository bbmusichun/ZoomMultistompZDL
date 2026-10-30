/*
 * BBAuto — post-NAM automatic level compensation.
 *
 * Placement: immediately AFTER NAM and BEFORE delay/reverb.
 * AMP mode: transparent, so the same chain can feed a physical amplifier.
 * NAM mode: a stereo-linked peak envelope gently reduces excessive level.
 *
 * The state is a 16-byte per-instance object in ctx[3]. No audio buffer,
 * allocation, lookup table, math library, divide, RMS square root or delay.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "bbauto_params.h"
#include "bbauto_dsp.h"

#ifndef BBAUTO_AUDIO_FUNC
#define BBAUTO_AUDIO_FUNC Fx_DLY_BBAuto
#endif

#define BBAUTO_DO_PRAGMA(x) _Pragma(#x)
#define BBAUTO_EXPAND_PRAGMA(x) BBAUTO_DO_PRAGMA(x)
#define BBAUTO_CODE_SECTION(f) BBAUTO_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

BBAUTO_CODE_SECTION(BBAUTO_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

void BBAUTO_AUDIO_FUNC(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *, *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    {
        float mode = zoom_param_norm01(params[BBAUTO_MODE_SLOT], BBAUTO_MODE_DEFAULT_NORM);
        float amount = zoom_param_norm01(params[BBAUTO_AMOUNT_SLOT], BBAUTO_AMOUNT_DEFAULT_NORM);
        volatile unsigned int *desc;
        uintptr_t base, end;
        BBAutoState *state;

        /* AMP mode leaves the samples untouched and does not need state. */
        if (mode < 0.5f || amount <= 0.0f) return;

        desc = ZDL_PTR(volatile unsigned int *, ctx[3]);
        if (!desc) return;
        base = (uintptr_t)desc[0];
        end = (uintptr_t)desc[1];
        if (base == 0u || end <= base || (base & 3u) != 0u) return;
        if (end - base < (uintptr_t)sizeof(BBAutoState)) return;

        state = (BBAutoState *)base;
        if (state->magic != BBAUTO_MAGIC ||
            state->version != BBAUTO_VERSION) {
            state->magic = 0u;
            state->version = BBAUTO_VERSION;
            state->envelope = 0.0f;
            state->gain = 1.0f;
            state->magic = BBAUTO_MAGIC;
        }

        bbauto_process(fxBuf, state, mode, amount);
    }
}
