#include "eye.h"
#include <math.h>
#include <string.h>

typedef struct { float r, g, b; } rgb_t;

static const rgb_t IRIS_COLOR[EYE_STATE_COUNT] = {
    [EYE_IDLE]     = {0.20f, 0.55f, 0.95f},
    [EYE_THINKING] = {0.62f, 0.38f, 0.98f},
    [EYE_WAITING]  = {1.00f, 0.62f, 0.10f},
    [EYE_DONE]     = {0.20f, 0.92f, 0.42f},
    [EYE_ERROR]    = {0.98f, 0.22f, 0.20f},
};

#define IRIS_ANGLE 0.42f        // iris radius, as an angle on the eyeball
#define DONE_SECONDS 4.0f       // "done" celebrates, then returns to idle

static struct {
    eye_state_t state;
    uint32_t state_since, now, last, next_blink, blink_start, rng;
    float look_x, look_y;       // target from the cursor
    float gaze_x, gaze_y;       // smoothed gaze
    float pupil;                // pupil size, fraction of iris
    float squint;               // 0..1 happy-eyes lower lid
    float glow;                 // status ring strength
    float phase;                // iris swirl
    float blink;                // 0 open .. 1 closed
    rgb_t iris;
} e;

static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static float smoothstep(float a, float b, float x) { x = clampf((x - a) / (b - a), 0, 1); return x * x * (3 - 2 * x); }
static float approach(float cur, float target, float k) { return cur + (target - cur) * k; }
static uint32_t rnd(void) { e.rng = e.rng * 1664525u + 1013904223u; return e.rng >> 8; }

void eye_init(uint32_t seed) {
    memset(&e, 0, sizeof e);
    e.rng = seed | 1;
    e.pupil = 0.45f;
    e.iris = IRIS_COLOR[EYE_IDLE];
}

void eye_look(float x, float y) {
    e.look_x = clampf(x, -1, 1);
    e.look_y = clampf(y, -1, 1);
}

void eye_set_state(eye_state_t s) {
    if (s >= EYE_STATE_COUNT) return;
    e.state = s;
    e.state_since = e.now;
}

void eye_update(uint32_t now) {
    float dt = e.last ? clampf((now - e.last) / 1000.0f, 0, 0.1f) : 0;
    e.last = e.now = now;
    float t = now / 1000.0f;
    float age = (now - e.state_since) / 1000.0f;
    if (e.state == EYE_DONE && age > DONE_SECONDS) eye_set_state(EYE_IDLE);

    float tx = e.look_x, ty = e.look_y, pupil = 0.45f, squint = 0, glow = 0.12f, swirl = 0.1f;
    switch (e.state) {
    case EYE_THINKING:  // glance up and away, iris swirls
        tx = 0.6f + 0.15f * sinf(t * 0.7f);
        ty = -0.6f + 0.1f * sinf(t * 1.1f);
        pupil = 0.4f; swirl = 1.5f;
        glow = 0.3f + 0.15f * sinf(t * 2);
        break;
    case EYE_WAITING:   // stare at you, pupils and ring pulse
        tx = ty = 0;
        pupil = 0.55f + 0.1f * sinf(t * 4);
        glow = 0.4f + 0.3f * sinf(t * 4);
        break;
    case EYE_DONE:      // happy squint, bright ring that fades
        pupil = 0.6f; squint = 1;
        glow = 0.8f * (1 - age / DONE_SECONDS);
        break;
    case EYE_ERROR:     // shake, tiny pupils
        tx = 0.3f * sinf(t * 30) * expf(-age * 2); ty = 0;
        pupil = 0.28f; glow = 0.5f;
        break;
    default: break;
    }

    float fast = 1 - expf(-dt * 12), slow = 1 - expf(-dt * 5);
    e.gaze_x = approach(e.gaze_x, tx, fast);
    e.gaze_y = approach(e.gaze_y, ty, fast);
    e.pupil = approach(e.pupil, pupil, slow);
    e.squint = approach(e.squint, squint, slow);
    e.glow = approach(e.glow, glow, slow);
    e.iris.r = approach(e.iris.r, IRIS_COLOR[e.state].r, slow);
    e.iris.g = approach(e.iris.g, IRIS_COLOR[e.state].g, slow);
    e.iris.b = approach(e.iris.b, IRIS_COLOR[e.state].b, slow);
    e.phase += dt * swirl;

    if (now >= e.next_blink) {
        e.blink_start = now;
        e.next_blink = now + 2500 + rnd() % 4000;
    }
    float bt = (now - e.blink_start) / 1000.0f;  // 0.15s closing, 0.15s opening
    e.blink = bt < 0.3f ? 1 - fabsf(bt - 0.15f) / 0.15f : 0;
}

static uint16_t rgb565(rgb_t c) {
    return (uint16_t)((int)(clampf(c.r, 0, 1) * 31 + 0.5f) << 11 |
                      (int)(clampf(c.g, 0, 1) * 63 + 0.5f) << 5 |
                      (int)(clampf(c.b, 0, 1) * 31 + 0.5f));
}

static rgb_t mix(rgb_t a, rgb_t b, float k) {
    return (rgb_t){a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k};
}

static rgb_t scale(rgb_t c, float k) { return (rgb_t){c.r * k, c.g * k, c.b * k}; }

void eye_render(uint16_t *fb) {
    // Gaze direction on the eyeball (screen axes: x right, y down, z toward viewer).
    float yaw = e.gaze_x * 0.55f, pitch = e.gaze_y * 0.45f;
    float gx = sinf(yaw) * cosf(pitch), gy = sinf(pitch), gz = cosf(yaw) * cosf(pitch);
    // Tangent basis around the gaze, for the iris fiber pattern.
    float n = sqrtf(gx * gx + gz * gz);
    float e1x = gz / n, e1z = -gx / n;
    float e2x = gy * e1z, e2y = gz * e1x - gx * e1z, e2z = -gy * e1x;

    const float iris_cos = cosf(IRIS_ANGLE), pupil_angle = IRIS_ANGLE * e.pupil;
    const float lx = -0.4f, ly = -0.5f, lz = 0.768f;  // light from upper left
    const rgb_t sclera = {0.93f, 0.93f, 0.95f}, dark = {0.02f, 0.02f, 0.03f}, white = {1, 1, 1};

    for (int j = 0; j < EYE_SIZE; j++) {
        float v = (j + 0.5f) / (EYE_SIZE / 2) - 1;
        for (int i = 0; i < EYE_SIZE; i++) {
            float u = (i + 0.5f) / (EYE_SIZE / 2) - 1;
            float r2 = u * u + v * v;
            if (r2 >= 1) { *fb++ = 0; continue; }
            float z = sqrtf(1 - r2), r = sqrtf(r2);

            // Eyelids, in screen space. Blink closes both; squint raises an arched lower lid.
            float top = -1.05f + 1.07f * e.blink + 0.12f * u * u * e.blink;
            float bottom = 1.05f - 1.03f * e.blink;
            float happy = 1.05f - e.squint * 0.6f * (1 - 0.5f * u * u);
            if (happy < bottom) bottom = happy;

            rgb_t col;
            if (v < top || v > bottom) {
                col = dark;
            } else {
                float c = u * gx + v * gy + z * gz;  // cos(angle from gaze center)
                col = sclera;
                if (c > iris_cos - 0.01f) {
                    float ang = acosf(clampf(c, -1, 1)), d = ang / IRIS_ANGLE;
                    rgb_t iris;
                    if (ang < pupil_angle) {
                        iris = dark;
                    } else {
                        float phi = atan2f(u * e2x + v * e2y + z * e2z, u * e1x + z * e1z) + e.phase;
                        float fiber = 0.8f + 0.2f * sinf(phi * 29 + d * 4) * sinf(phi * 11 - d * 7);
                        float limbal = 1 - 0.7f * smoothstep(0.82f, 1, d);
                        iris = scale(e.iris, fiber * (0.7f + 0.5f * d) * limbal);
                    }
                    col = mix(sclera, iris, smoothstep(iris_cos - 0.01f, iris_cos + 0.01f, c));
                }
                // Sphere shading plus a wet highlight sell the 3D look.
                float diffuse = fmaxf(0, u * lx + v * ly + z * lz);
                col = scale(col, (0.35f + 0.65f * diffuse) * (0.55f + 0.45f * z));
                float spec = 0.8f * expf(-((u + 0.38f) * (u + 0.38f) + (v + 0.42f) * (v + 0.42f)) * 40);
                col = mix(col, white, spec);
            }

            // Status ring around the rim, visible even mid-blink.
            col = mix(col, e.iris, smoothstep(0.86f, 1, r) * clampf(e.glow, 0, 1));
            *fb++ = rgb565(col);
        }
    }
}
