/*
 * BBBoost — clean input booster for a real amp or a NAM profile.
 *
 * Placement: before the amp / NAM.
 * DSP: one shared gain multiplication for the 16-sample stereo block.
 * No clipping, nonlinear stage, envelope, buffer, heap or persistent state.
 */

#include <stdint.h>

#include "../../airwindows/common/zoom_params.h"
#include "bbboost_params.h"
#include "bbboost_dsp.h"

#ifndef BBBOOST_AUDIO_FUNC
#define BBBOOST_AUDIO_FUNC Fx_DLY_BBBoost
#endif

#define BBBOOST_DO_PRAGMA(x) _Pragma(#x)
#define BBBOOST_EXPAND_PRAGMA(x) BBBOOST_DO_PRAGMA(x)
#define BBBOOST_CODE_SECTION(f) BBBOOST_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))

BBBOOST_CODE_SECTION(BBBOOST_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

void BBBOOST_AUDIO_FUNC(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);

    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *, *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    {
        float gain = zoom_param_norm01(params[BBBOOST_GAIN_SLOT], BBBOOST_GAIN_DEFAULT_NORM);
        float level = zoom_param_norm01(params[BBBOOST_LEVEL_SLOT], BBBOOST_LEVEL_DEFAULT_NORM);
        bbboost_process(fxBuf, gain, level);
    }
}
