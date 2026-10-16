// Look 3: Sphere-style emoji, a flat yellow face on a ball that turns toward the cursor.
#include "face.h"
#include <math.h>
#include <string.h>

typedef struct { float r, g, b; } rgb_t;

#define DONE_SECONDS 4.0f                // "done" celebrates, then returns to idle

static struct {
    face_state_t state;
    uint32_t state_since, now, last, next_blink, blink_start, rng;
    float look_x, look_y;   // target from the cursor
    float gaze_x, gaze_y;   // smoothed: where the face points
    float blink;            // 0 open .. 1 closed
    float bright;           // overall brightness
    float tint;             // 0 yellow .. 1 angry red
} f;

static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static float smoothstep(float a, float b, float x) { x = clampf((x - a) / (b - a), 0, 1); return x * x * (3 - 2 * x); }
static float approach(float cur, float target, float k) { return cur + (target - cur) * k; }
static uint32_t rnd(void) { f.rng = f.rng * 1664525u + 1013904223u; return f.rng >> 8; }
static rgb_t mix(rgb_t a, rgb_t b, float k) { return (rgb_t){a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k}; }
static rgb_t scale(rgb_t c, float k) { return (rgb_t){c.r * k, c.g * k, c.b * k}; }

void face_init(uint32_t seed) {
    memset(&f, 0, sizeof f);
    f.rng = seed | 1;
    f.bright = 1;
}

void face_look(float x, float y) {
    f.look_x = clampf(x, -1, 1);
    f.look_y = clampf(y, -1, 1);
}

void face_set_state(face_state_t s) {
    if (s >= FACE_STATE_COUNT) return;
    f.state = s;
    f.state_since = f.now;
}

void face_update(uint32_t now) {
    float dt = f.last ? clampf((now - f.last) / 1000.0f, 0, 0.1f) : 0;
    f.last = f.now = now;
    float t = now / 1000.0f;
    float age = (now - f.state_since) / 1000.0f;
    if (f.state == FACE_DONE && age > DONE_SECONDS) face_set_state(FACE_IDLE);

    float tx = f.look_x, ty = f.look_y, bright = 1, tint = 0;
    switch (f.state) {
    case FACE_THINKING:  // look up and away, drifting
        tx = 0.55f + 0.15f * sinf(t * 0.7f);
        ty = -0.55f + 0.1f * sinf(t * 1.1f);
        break;
    case FACE_WAITING:   // turn to face you, pulse to get attention
        tx = ty = 0;
        bright = 0.7f + 0.3f * sinf(t * 4);
        break;
    case FACE_ERROR:     // shake, go red
        tx = 0.35f * sinf(t * 30) * expf(-age * 2); ty = 0;
        tint = 1;
        break;
    default: break;
    }

    float fast = 1 - expf(-dt * 10), slow = 1 - expf(-dt * 5);
    f.gaze_x = approach(f.gaze_x, tx, fast);
    f.gaze_y = approach(f.gaze_y, ty, fast);
    f.bright = approach(f.bright, bright, fast);
    f.tint = approach(f.tint, tint, slow);

    if (now >= f.next_blink) {
        f.blink_start = now;
        f.next_blink = now + 2500 + rnd() % 4000;
    }
    float bt = (now - f.blink_start) / 1000.0f;  // 0.12s closing, 0.12s opening
    f.blink = bt < 0.24f ? 1 - fabsf(bt - 0.12f) / 0.12f : 0;
}

// --- Feature shapes, as signed distances in face coordinates (x right, y down, face radius 1) ---

static float len(float x, float y) { return sqrtf(x * x + y * y); }

static float ellipse(float x, float y, float cx, float cy, float rx, float ry) {
    float m = rx < ry ? rx : ry;
    return (len((x - cx) / rx, (y - cy) / ry) - 1) * m;
}

static float segment(float x, float y, float ax, float ay, float bx, float by, float w) {
    float px = x - ax, py = y - ay, dx = bx - ax, dy = by - ay;
    float h = clampf((px * dx + py * dy) / (dx * dx + dy * dy), 0, 1);
    return len(px - dx * h, py - dy * h) - w;
}

// Stroke along a circle, kept on one side of the line y = cut (below if below > 0).
static float arc(float x, float y, float cx, float cy, float r, float w, float cut, int below) {
    float d = fabsf(len(x - cx, y - cy) - r) - w;
    float side = below ? cut - y : y - cut;
    return d > side ? d : side;
}

static float minf(float a, float b) { return a < b ? a : b; }

// Ink amount (0..1) for dark features, and for the red tongue.
static void features(float x, float y, float *ink, float *tongue) {
    float eye_ry = 0.16f * (1 - f.blink) + 0.02f, d = 1, t = 1;
    switch (f.state) {
    case FACE_IDLE:
        d = minf(ellipse(x, y, -0.3f, -0.18f, 0.1f, eye_ry), ellipse(x, y, 0.3f, -0.18f, 0.1f, eye_ry));
        d = minf(d, arc(x, y, 0, -0.05f, 0.45f, 0.045f, 0.22f, 1));
        break;
    case FACE_THINKING:
        d = minf(ellipse(x, y, -0.28f, -0.2f, 0.09f, eye_ry * 0.9f), ellipse(x, y, 0.28f, -0.2f, 0.09f, eye_ry * 0.9f));
        d = minf(d, segment(x, y, -0.42f, -0.44f, -0.18f, -0.44f, 0.035f));  // flat brow
        d = minf(d, segment(x, y, 0.16f, -0.5f, 0.42f, -0.58f, 0.035f));     // raised brow
        d = minf(d, segment(x, y, -0.1f, 0.36f, 0.2f, 0.32f, 0.04f));        // hmm mouth
        break;
    case FACE_WAITING:
        d = minf(ellipse(x, y, -0.3f, -0.16f, 0.12f, eye_ry * 1.2f), ellipse(x, y, 0.3f, -0.16f, 0.12f, eye_ry * 1.2f));
        d = minf(d, arc(x, y, -0.3f, -0.3f, 0.16f, 0.035f, -0.4f, 0));       // raised brows
        d = minf(d, arc(x, y, 0.3f, -0.3f, 0.16f, 0.035f, -0.4f, 0));
        d = minf(d, ellipse(x, y, 0, 0.36f, 0.08f, 0.1f));                    // "o" mouth
        break;
    case FACE_DONE: {
        d = minf(arc(x, y, -0.3f, -0.1f, 0.12f, 0.045f, -0.12f, 0), arc(x, y, 0.3f, -0.1f, 0.12f, 0.045f, -0.12f, 0));
        float grin = len(x, y - 0.12f) - 0.45f;                               // open grin: lower half-disc
        grin = grin > 0.14f - y ? grin : 0.14f - y;
        d = minf(d, grin);
        t = grin > ellipse(x, y, 0, 0.5f, 0.2f, 0.12f) ? grin : ellipse(x, y, 0, 0.5f, 0.2f, 0.12f);
        break;
    }
    case FACE_ERROR:
        d = minf(ellipse(x, y, -0.3f, -0.15f, 0.1f, eye_ry), ellipse(x, y, 0.3f, -0.15f, 0.1f, eye_ry));
        d = minf(d, segment(x, y, -0.46f, -0.46f, -0.16f, -0.34f, 0.04f));   // angry brows
        d = minf(d, segment(x, y, 0.46f, -0.46f, 0.16f, -0.34f, 0.04f));
        d = minf(d, arc(x, y, 0, 0.62f, 0.3f, 0.045f, 0.5f, 0));             // frown
        break;
    default: break;
    }
    *ink = smoothstep(0.02f, -0.02f, d);
    *tongue = smoothstep(0.02f, -0.02f, t);
}

// Color of the ball at a screen point (u, v inside the unit circle).
static rgb_t ball(float u, float v, float sy, float cy, float sp, float cp) {
    float z = sqrtf(1 - u * u - v * v);
    // Rotate the screen point into face coordinates (undo yaw, then pitch).
    float x1 = u * cy - z * sy, z1 = u * sy + z * cy;
    float y2 = v * cp - z1 * sp, z2 = v * sp + z1 * cp;

    const rgb_t yellow = {1.0f, 0.82f, 0.1f}, orange = {0.95f, 0.42f, 0.02f}, red = {0.95f, 0.15f, 0.05f};
    const rgb_t ink_col = {0.08f, 0.03f, 0.0f}, tongue_col = {0.9f, 0.25f, 0.3f};
    rgb_t col = mix(mix(orange, yellow, smoothstep(0, 0.8f, z)), red, f.tint * 0.7f);
    float spec = 0.5f * expf(-((u + 0.35f) * (u + 0.35f) + (v + 0.4f) * (v + 0.4f)) * 18);
    col = mix(col, (rgb_t){1, 1, 0.9f}, spec);  // shine on the skin only, under the features

    if (z2 > 0) {
        float ink, tongue;
        features(x1, y2, &ink, &tongue);
        col = mix(col, ink_col, ink);
        col = mix(col, tongue_col, tongue);
    }
    float diffuse = fmaxf(0, -0.4f * u - 0.5f * v + 0.768f * z);
    return scale(col, 0.45f + 0.55f * diffuse);
}

static uint16_t rgb565(rgb_t c) {
    return (uint16_t)((int)(clampf(c.r, 0, 1) * 31 + 0.5f) << 11 |
                      (int)(clampf(c.g, 0, 1) * 63 + 0.5f) << 5 |
                      (int)(clampf(c.b, 0, 1) * 31 + 0.5f));
}

void face_render(uint16_t *fb) {
    float yaw = f.gaze_x * 0.6f, pitch = f.gaze_y * 0.5f;
    float sy = sinf(yaw), cy = cosf(yaw), sp = sinf(pitch), cp = cosf(pitch);

    for (int j = 0; j < FACE_SIZE; j++) {
        float v = (j + 0.5f) / (FACE_SIZE / 2) - 1;
        for (int i = 0; i < FACE_SIZE; i++) {
            float u = (i + 0.5f) / (FACE_SIZE / 2) - 1;
            *fb++ = u * u + v * v < 1 ? rgb565(scale(ball(u, v, sy, cy, sp, cp), f.bright)) : 0;
        }
    }
}
