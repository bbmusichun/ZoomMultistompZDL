/* Skull v0.03: a shaded three-quarter skull that reacts to the signal
 * (MS-70CDR 2.10).
 *
 * Audio passes through untouched. A fast-attack follower (release set by
 * Decay) drives it: the jaw opens through SKULL_JAW_STEPS pre-rendered frames
 * (our own 3D model, lit and dithered: skull3d.py), the eye lights appear,
 * grow and flicker with level, and a sharp attack jolts the skull sideways for
 * a few frames. Full screen: the knob boxes are hidden at start-up
 * (zv_hide_knob_boxes, experimental); the knobs still work on the knob pages. At the very loudest
 * (Boom) it explodes: a flash, ~100 bone fragments (precomputed start and
 * velocity, build.py) fly out under gravity with a shockwave ring, lie still,
 * then fly back and the skull re-forms; it re-arms once the level drops. One frame copy per
 * callback step, then zvis encodes and shows it (~30 fps), only while this
 * card is on screen. See ../common/zvis.h. */
#include <stdint.h>
#include "zvis.h"
#include "skull_art.h"

extern unsigned int _Fx_DLY_Skull_Info[];

#define MAGIC 0x534B5533u                      /* 'SKU3' */
#define FXID  416u
#define BOOM_T   26u                           /* frames of flying fragments */
#define PAUSE_T  8u                            /* debris lying still */
#define REFORM_T 14u                           /* fragments flying home */
#define GRAV     0.09f                         /* px/frame^2 */
#define FRAG_STEPS 4u                          /* callbacks to draw one frame's fragments */

enum { FACE, BOOM, PAUSE, REFORM };

typedef struct {
    unsigned magic, step, shake, flip, tick, mode, t, armed, rest;
    int dx;
    float env, slow, lvl;
    ZvState z;
} SkullState;

/* Panel rows 0..SKULL_PANEL to black (v=1) or white (v=0); dials untouched. */
ZV_INLINE(panel)
static inline void panel(ZvState *z, int v)
{
    unsigned b, x;
    for (b = 0; 8u * b <= (unsigned)SKULL_PANEL; ++b) {
        unsigned char m = 8u * b + 7u <= (unsigned)SKULL_PANEL ? 0xFFu
                        : (unsigned char)((1u << (SKULL_PANEL - 8 * (int)b + 1)) - 1u);
        for (x = 0; x < (unsigned)ZV_W; ++x) {
            unsigned char *p = &z->frame[b * ZV_W + x];
            *p = v ? (unsigned char)(*p | m) : (unsigned char)(*p & (unsigned char)~m);
        }
    }
}

/* Fragment i at frame t of the blast (float pixels). */
ZV_INLINE(frag_at)
static inline void frag_at(unsigned i, float t, float *x, float *y)
{
    float vx = (float)(int)(signed char)SKULL_FVX[i] * (1.0f / 16.0f);
    float vy = (float)(int)(signed char)SKULL_FVY[i] * (1.0f / 16.0f);
    float fy = (float)(int)SKULL_FY[i] + vy * t + 0.5f * GRAV * t * t;
    if (fy > (float)(SKULL_PANEL - 1)) fy = (float)(SKULL_PANEL - 1);          /* land on the floor */
    *x = (float)(int)SKULL_FX[i] + vx * t;
    *y = fy;
}

/* A 2x2 bone chunk, white, kept inside the panel. */
ZV_INLINE(chunk)
static inline void chunk(ZvState *z, int x, int y)
{
    if (y < 0 || y > SKULL_PANEL - 1) return;
    zv_px(z, x, y, 0); zv_px(z, x + 1, y, 0); zv_px(z, x, y + 1, 0); zv_px(z, x + 1, y + 1, 0);
}

/* Copy a frame, the panel rows (0..SKULL_PANEL) shifted right by dx; the rest as is. */
ZV_INLINE(copy_shifted)
static inline void copy_shifted(ZvState *z, const unsigned char *src, int dx)
{
    unsigned b, x;
    for (b = 0; b < 8u; ++b)
        for (x = 0; x < (unsigned)ZV_W; ++x) {
            int sx = (int)x - dx;
            unsigned char v = src[b * ZV_W + x];
            unsigned char s = (sx >= 0 && sx < ZV_W) ? src[b * ZV_W + (unsigned)sx] : 0xFFu;
            if (8u * b + 7u <= (unsigned)SKULL_PANEL) v = s;                  /* whole block in the panel */
            else if (8u * b <= (unsigned)SKULL_PANEL) {                       /* split block */
                unsigned char m = (unsigned char)((1u << (SKULL_PANEL - 8 * (int)b + 1)) - 1u);
                v = (unsigned char)((s & m) | (v & (unsigned char)~m));
            }
            z->frame[b * ZV_W + x] = v;
        }
}

/* Eye light: a white point growing to a small diamond with level r (0..3). */
ZV_INLINE(eye)
static inline void eye(ZvState *z, int x, int y, int r)
{
    int i, j;
    for (j = -r; j <= r; ++j)
        for (i = -r; i <= r; ++i)
            if ((i < 0 ? -i : i) + (j < 0 ? -j : j) <= r) zv_px(z, x + i, y + j, 0);
}

#pragma CODE_SECTION(Fx_DLY_Skull, ".audio")
void Fx_DLY_Skull(uintptr_t *ctx)
{
    float *params = (float *)ctx[1];
    float *fx = (float *)ctx[5];
    uintptr_t *desc = (uintptr_t *)ctx[3];
    unsigned int *magic_src = (unsigned int *)ctx[12];
    unsigned int *magic_dst = *(unsigned int **)ctx[11];
    unsigned int *info = _Fx_DLY_Skull_Info;
    uintptr_t base, end;
    SkullState *st;
    unsigned i;
    int visible;
    *magic_dst = *magic_src;
    if (!desc) return;
    base = desc[0]; end = desc[1];
    if (!base || end <= base) return;
    base += (8u - (base & 7u)) & 7u;
    if (end - base < sizeof(SkullState)) return;
    st = (SkullState *)base;
    if (st->magic != MAGIC) {
        st->magic = MAGIC; st->step = 0; st->shake = 0; st->flip = 0; st->tick = 0; st->dx = 0;
        st->mode = FACE; st->t = 0; st->armed = 1; st->rest = 0;
        st->env = 0.0f; st->slow = 0.0f; st->lvl = 0.0f;
        zv_init(&st->z, FXID, info);
        zv_hide_knob_boxes(&st->z, info);      /* full screen (experimental) */
        return;
    }

    visible = zv_visible(&st->z, ctx);
    zv_service(&st->z, info, visible);
    if (params[0] < 0.5f || !visible) { st->step = 0; return; }

    {   /* follow the signal every callback */
        float d = zoom_param_norm01(params[6], 0.4f);
        float rel = 0.002f * (1.0f - d) + 0.00002f;
        float sense = zoom_param_norm01(params[5], 0.5f);
        float boom = zoom_param_norm01(params[8], 0.5f);
        float thr = 0.62f * (1.0f - boom) + 0.08f;  /* env needed to explode; boom 0 = never */
        for (i = 0; i < 8u; ++i) {
            float s = (fx[i] + fx[i + 8]) * 0.5f;
            st->env = zv_follow(st->env, s, 0.02f, rel);
            st->slow = zv_follow(st->slow, s, 0.0005f, 0.0005f);
        }
        st->lvl = st->env * (1.0f + sense * sense * 30.0f);
        if (!(st->lvl <= 1.5f)) st->lvl = 1.5f;
        if (st->mode == FACE && st->env - 1.6f * st->slow > 0.03f && st->shake == 0u)
            st->shake = (unsigned)(int)(zoom_param_norm01(params[7], 0.5f) * 8.0f);   /* frames; 0 = off */
        /* Re-arm only with the face back on screen, the level well down, and
         * ~1 s (REST_CB callbacks) since it re-formed. */
        if (st->mode == FACE && st->rest < 5512u) st->rest++;
        if (st->mode == FACE && st->rest >= 5512u && st->env < thr * 0.6f) st->armed = 1;
        if (st->mode == FACE && st->armed && boom > 0.004f && st->env > thr) {
            st->mode = BOOM; st->t = 0; st->armed = 0; st->shake = 0;
        }
    }
    if (st->z.phase != ZV_IDLE) return;

    if (st->mode != FACE) {                     /* the blast, the pause, re-forming */
        if (st->step == 0u) {
            panel(&st->z, st->mode == BOOM && st->t == 0u ? 0 : 1);   /* white flash on frame 0 */
            if (st->mode == BOOM && st->t >= 1u && st->t <= 9u) {     /* shockwave */
                int r = 4 + 6 * (int)st->t, a;
                for (a = 0; a < 64; a += 2) {
                    int x = SKULL_CX + (r * (int)(signed char)SKULL_COS64[a]) / 127;
                    int y = SKULL_CY - (r * (int)(signed char)SKULL_COS64[(a + 48) & 63]) / 178;   /* /127*1.4 */
                    if (y >= 0 && y <= SKULL_PANEL) zv_px(&st->z, x, y, 0);
                }
            }
            st->step = 1;
            return;
        }
        {   /* steps 1..FRAG_STEPS: a share of the fragments each, so no callback draws them all */
            float t = (float)(int)st->t;
            unsigned keep = SKULL_FRAGS;
            unsigned lo = ((st->step - 1u) * SKULL_FRAGS) / FRAG_STEPS, hi = (st->step * SKULL_FRAGS) / FRAG_STEPS;        /* fragments thin out late in the blast */
            if (st->mode == BOOM && st->t > 12u) keep = SKULL_FRAGS - (SKULL_FRAGS * (st->t - 12u)) / (BOOM_T + 6u);
            if (!(st->mode == BOOM && st->t == 0u))
                for (i = lo; i < hi; ++i) {
                    float x, y;
                    if (st->mode != REFORM && (i * 37u) % SKULL_FRAGS >= keep) continue;
                    if (st->mode == BOOM) frag_at(i, t, &x, &y);
                    else {
                        frag_at(i, (float)(int)BOOM_T, &x, &y);
                        if (st->mode == REFORM) {           /* ease back home */
                            float u = t * (1.0f / (float)(int)REFORM_T);
                            float e = u * u * (3.0f - 2.0f * u);
                            x += ((float)(int)SKULL_FX[i] - x) * e;
                            y += ((float)(int)SKULL_FY[i] - y) * e;
                        }
                    }
                    chunk(&st->z, (int)(x + 0.5f), (int)(y + 0.5f));
                }
            if (st->step < FRAG_STEPS) { st->step++; return; }
            zv_submit(&st->z);
            st->step = 0;
            if (++st->t >= (st->mode == BOOM ? BOOM_T : st->mode == PAUSE ? PAUSE_T : REFORM_T)) {
                st->t = 0;
                st->mode = st->mode == BOOM ? PAUSE : st->mode == PAUSE ? REFORM : FACE;
                if (st->mode == FACE) { st->armed = 0; st->rest = 0; }
            }
        }
        return;
    }

    if (st->step == 0u) {                       /* step 1: the frame for this jaw opening, shaken */
        int k = (int)(st->lvl * (float)SKULL_JAW_STEPS);
        if (k > (int)SKULL_JAW_STEPS - 1) k = (int)SKULL_JAW_STEPS - 1;
        if (k < 0) k = 0;
        st->dx = 0;
        if (st->shake) { st->flip ^= 1u; st->dx = st->flip ? -2 : 2; st->shake--; }
        copy_shifted(&st->z, SKULL_FRAMES + 1024u * (unsigned)k, st->dx);
        st->step = 1;
        return;
    }
    {   /* step 2: eye lights, flickering a little with level */
        float lvl = st->lvl;
        int r = lvl > 1.0f ? 3 : lvl > 0.6f ? 2 : lvl > 0.3f ? 1 : lvl > 0.12f ? 0 : -1;
        st->tick++;
        if (r >= 1 && (st->tick & 3u) == 0u) r--;   /* flicker */
        if (r >= 0) {
            eye(&st->z, SKULL_EYE_LX + st->dx, SKULL_EYE_LY, r);
            eye(&st->z, SKULL_EYE_RX + st->dx, SKULL_EYE_RY, r);
        }
        zv_submit(&st->z);
        st->step = 0;
    }
}
