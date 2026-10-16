// Eye renderer. Portable C with no platform deps: runs in the browser sim (wasm) and on the ESP32.
#pragma once
#include <stdint.h>

#define EYE_SIZE 240  // 240x240 round LCD (GC9A01)

typedef enum { EYE_IDLE, EYE_THINKING, EYE_DONE, EYE_ERROR, EYE_STATE_COUNT } eye_state_t;

void eye_init(uint32_t seed);
void eye_look(float x, float y);  // -1..1, where the cursor is relative to screen center
void eye_set_state(eye_state_t s);
void eye_set_half_lid(int on);  // experiment
void eye_update(uint32_t now_ms);
void eye_render(uint16_t *fb);    // fills EYE_SIZE*EYE_SIZE RGB565 pixels
