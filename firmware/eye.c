// Look 1: minimalist glowing white face (pill eyes, small smile) on a black glass ball.
// Expressions morph into each other instead of snapping.
#include "eye.h"
#include <math.h>
#include <string.h>

typedef struct { float r, g, b; } rgb_t;

#define DONE_SECONDS 4.0f   // "done" celebrates, then returns to idle
#define MORPH_SECONDS 0.3f  // expression change duration
#define AA 0.012f           // edge softness, about one pixel in face units

static struct {
    eye_state_t state, prev;
    uint32_t state_since, now, last, next_blink, blink_start, rng;
    float look_x, look_y;   // target from the cursor
    float gaze_x, gaze_y;   // smoothed: where the face points
    float blink;            // 0 open .. 1 closed
    float bright;           // overall brightness
    float morph;            // 0 = showing prev expression .. 1 = fully current
} e;

static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static float smoothstep(float a, float b, float x) { x = clampf((x - a) / (b - a), 0, 1); return x * x * (3 - 2 * x); }
static float approach(float cur, float target, float k) { return cur + (target - cur) * k; }
static uint32_t rnd(void) { e.rng = e.rng * 1664525u + 1013904223u; return e.rng >> 8; }
static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }
static float len(float x, float y) { return sqrtf(x * x + y * y); }

void eye_init(uint32_t seed) {
    memset(&e, 0, sizeof e);
    e.rng = seed | 1;
    e.bright = 1;
    e.morph = 1;
}

void eye_look(float x, float y) {
    e.look_x = clampf(x, -1, 1);
    e.look_y = clampf(y, -1, 1);
}

void eye_set_state(eye_state_t s) {
    if (s >= EYE_STATE_COUNT || s == e.state) return;
    e.prev = e.state;
    e.state = s;
    e.state_since = e.now;
}

void eye_update(uint32_t now) {
    float dt = e.last ? clampf((now - e.last) / 1000.0f, 0, 0.1f) : 0;
    e.last = e.now = now;
    float t = now / 1000.0f;
    float age = (now - e.state_since) / 1000.0f;
    if (e.state == EYE_DONE && age > DONE_SECONDS) eye_set_state(EYE_IDLE);

    float tx = e.look_x, ty = e.look_y, bright = 1;
    switch (e.state) {
    case EYE_THINKING:  // look up and away, drifting
        tx = 0.55f + 0.15f * sinf(t * 0.7f);
        ty = -0.55f + 0.1f * sinf(t * 1.1f);
        break;
    case EYE_WAITING:   // turn to face you, pulse to get attention
        tx = ty = 0;
        bright = 0.7f + 0.3f * sinf(t * 4);
        break;
    case EYE_ERROR:     // shake
        tx = 0.35f * sinf(t * 30) * expf(-age * 2); ty = 0;
        break;
    default: break;
    }

    float fast = 1 - expf(-dt * 10);
    e.gaze_x = approach(e.gaze_x, tx, fast);
    e.gaze_y = approach(e.gaze_y, ty, fast);
    e.bright = approach(e.bright, bright, fast);
    e.morph = smoothstep(0, MORPH_SECONDS, age);

    if (now >= e.next_blink) {
        e.blink_start = now;
        e.next_blink = now + 2500 + rnd() % 4000;
    }
    float bt = (now - e.blink_start) / 1000.0f;  // 0.12s closing, 0.12s opening
    e.blink = bt < 0.24f ? 1 - fabsf(bt - 0.12f) / 0.12f : 0;
}

// --- Feature shapes, as signed distances in face coordinates (x right, y down, face radius 1) ---

static float pill(float x, float y, float cx, float cy, float hw, float hh, float r) {
    float qx = fabsf(x - cx) - hw, qy = fabsf(y - cy) - hh;
    return len(maxf(qx, 0), maxf(qy, 0)) + minf(maxf(qx, qy), 0) - r;
}

static float segment(float x, float y, float ax, float ay, float bx, float by, float w) {
    float px = x - ax, py = y - ay, dx = bx - ax, dy = by - ay;
    float h = clampf((px * dx + py * dy) / (dx * dx + dy * dy), 0, 1);
    return len(px - dx * h, py - dy * h) - w;
}

// Stroke along a circle, kept on one side of the line y = cut (below if below > 0).
static float arc(float x, float y, float cx, float cy, float r, float w, float cut, int below) {
    float d = fabsf(len(x - cx, y - cy) - r) - w;
    return maxf(d, below ? cut - y : y - cut);
}

static float eyes(float x, float y, float hh) {
    return minf(pill(x, y, -0.26f, -0.12f, 0.035f, hh, 0.06f), pill(x, y, 0.26f, -0.12f, 0.035f, hh, 0.06f));
}

static float features(eye_state_t state, float x, float y) {
    float open = 0.07f * (1 - e.blink);  // eye half-height above the rounded caps
    switch (state) {
    case EYE_THINKING:  // one eye squints, mouth slides to the side
        return minf(minf(pill(x, y, -0.26f, -0.12f, 0.035f, open, 0.06f),
                         pill(x, y, 0.26f, -0.1f, 0.035f, open * 0.2f, 0.05f)),
                    segment(x, y, 0.04f, 0.27f, 0.2f, 0.25f, 0.03f));
    case EYE_WAITING:   // big round eyes, small "o"
        return minf(eyes(x, y, open * 0.6f + 0.02f), fabsf(len(x, y - 0.28f) - 0.05f) - 0.025f);
    case EYE_DONE:      // ^ ^ eyes, wide smile
        return minf(minf(arc(x, y, -0.26f, -0.06f, 0.1f, 0.035f, -0.08f, 0), arc(x, y, 0.26f, -0.06f, 0.1f, 0.035f, -0.08f, 0)),
                    arc(x, y, 0, 0.0f, 0.3f, 0.035f, 0.14f, 1));
    case EYE_ERROR: {   // x x eyes, frown
        float d = 1;
        for (int s = -1; s <= 1; s += 2) {
            float cx = 0.26f * s;
            d = minf(d, segment(x, y, cx - 0.08f, -0.2f, cx + 0.08f, -0.04f, 0.03f));
            d = minf(d, segment(x, y, cx - 0.08f, -0.04f, cx + 0.08f, -0.2f, 0.03f));
        }
        return minf(d, arc(x, y, 0, 0.45f, 0.2f, 0.03f, 0.33f, 0));
    }
    default:            // idle: pill eyes, small smile
        return minf(eyes(x, y, open), arc(x, y, 0, 0.04f, 0.2f, 0.03f, 0.16f, 1));
    }
}

// Color of the ball at a screen point (u, v inside the unit circle).
static rgb_t ball(float u, float v, float sy, float cy, float sp, float cp) {
    float r2 = u * u + v * v, z = sqrtf(1 - r2);
    // Rotate the screen point into face coordinates (undo yaw, then pitch).
    float x1 = u * cy - z * sy, z1 = u * sy + z * cy;
    float y2 = v * cp - z1 * sp, z2 = v * sp + z1 * cp;

    // Black glass: faint top-left sheen and a thin rim of light so the sphere reads.
    float sheen = 0.06f * expf(-((u + 0.35f) * (u + 0.35f) + (v + 0.45f) * (v + 0.45f)) * 6);
    float rim = 0.08f * smoothstep(0.9f, 1.0f, sqrtf(r2)) * (0.5f - 0.5f * v);
    float bg = 0.015f + sheen + rim;
    rgb_t col = {bg, bg, bg * 1.1f};

    if (z2 > 0) {
        float d = features(e.state, x1, y2);
        if (e.morph < 1) d = features(e.prev, x1, y2) + (d - features(e.prev, x1, y2)) * e.morph;
        float lit = smoothstep(AA, -AA, d) + 0.18f * expf(-maxf(d, 0) * 18);  // crisp core + soft bloom
        lit *= (0.5f + 0.5f * z2) * e.bright;  // dimmer as features turn away
        const rgb_t white = {0.96f, 0.97f, 1.0f};
        col.r += white.r * lit; col.g += white.g * lit; col.b += white.b * lit;
    }
    return col;
}

// RGB565 has visible steps in dark shades; a 4x4 ordered dither hides the banding.
static uint16_t rgb565(rgb_t c, int x, int y) {
    static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    float t = BAYER[y & 3][x & 3] / 16.0f;
    return (uint16_t)((int)(clampf(c.r, 0, 1) * 31 + t) << 11 |
                      (int)(clampf(c.g, 0, 1) * 63 + t) << 5 |
                      (int)(clampf(c.b, 0, 1) * 31 + t));
}

void eye_render(uint16_t *fb) {
    float yaw = e.gaze_x * 0.6f, pitch = e.gaze_y * 0.5f;
    float sy = sinf(yaw), cy = cosf(yaw), sp = sinf(pitch), cp = cosf(pitch);
    for (int j = 0; j < EYE_SIZE; j++) {
        float v = (j + 0.5f) / (EYE_SIZE / 2) - 1;
        for (int i = 0; i < EYE_SIZE; i++) {
            float u = (i + 0.5f) / (EYE_SIZE / 2) - 1;
            *fb++ = u * u + v * v < 1 ? rgb565(ball(u, v, sy, cy, sp, cp), i, j) : 0;
        }
    }
}
