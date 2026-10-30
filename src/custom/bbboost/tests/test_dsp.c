#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../bbboost_dsp.h"
#include "../../bbauto/bbauto_dsp.h"

static void fill_stereo(float *buffer, float left, float right)
{
    int i;
    for (i = 0; i < 8; ++i) {
        buffer[i] = left;
        buffer[i + 8] = right;
    }
}

int main(void)
{
    float boost[16];
    float level_buffer[16];
    BBAutoState state = {BBAUTO_MAGIC, BBAUTO_VERSION, 0.0f, 1.0f};
    float compressed_gain;
    int i, block;

    fill_stereo(boost, 0.125f, -0.125f);
    bbboost_process(boost, 0.0f, 1.0f);
    for (i = 0; i < 16; ++i) assert(fabsf(boost[i] - (i < 8 ? 0.125f : -0.125f)) < 1.0e-7f);

    fill_stereo(boost, 1.0f, -1.0f);
    bbboost_process(boost, 1.0f, 1.0f);
    assert(fabsf(boost[0] - 7.943282f) < 1.0e-5f);
    assert(fabsf(boost[8] + 7.943282f) < 1.0e-5f);

    fill_stereo(boost, 1.0f, -1.0f);
    bbboost_process(boost, 1.0f, 0.5f);
    assert(fabsf(boost[0] - 3.971641f) < 1.0e-5f);

    /* AMP must be transparent and must not alter the leveler's state. */
    fill_stereo(level_buffer, 0.4f, -0.2f);
    bbauto_process(level_buffer, &state, 0.0f, 0.8f);
    assert(fabsf(level_buffer[0] - 0.4f) < 1.0e-7f);
    assert(fabsf(level_buffer[8] + 0.2f) < 1.0e-7f);
    assert(state.envelope == 0.0f && state.gain == 1.0f);

    /* NAM mode: sustained high output is reduced; stereo image stays linked. */
    for (block = 0; block < 1200; ++block) {
        fill_stereo(level_buffer, 0.8f, 0.4f);
        bbauto_process(level_buffer, &state, 1.0f, 0.6f);
    }
    assert(state.envelope > 0.79f && state.envelope <= 0.81f);
    assert(state.gain < 0.9f && state.gain >= (1.0f - BBAUTO_MAX_REDUCTION * 0.6f) - 0.001f);
    assert(fabsf(level_buffer[0] / level_buffer[8] - 2.0f) < 1.0e-4f);
    compressed_gain = state.gain;

    /* Lower signal lets the gain recover smoothly rather than remain clamped. */
    for (block = 0; block < 3500; ++block) {
        fill_stereo(level_buffer, 0.05f, 0.025f);
        bbauto_process(level_buffer, &state, 1.0f, 0.6f);
    }
    assert(state.gain > compressed_gain);
    assert(state.gain > 0.85f && state.gain <= 1.0f);
    assert(isfinite(state.envelope) && isfinite(state.gain));

    puts("BB Boost DSP tests: PASS");
    return 0;
}
