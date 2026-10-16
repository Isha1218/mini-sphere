// Face renderer. Portable C with no platform deps: runs in the browser sim (wasm) and on the ESP32.
#pragma once
#include <stdint.h>

#define FACE_SIZE 240  // 240x240 round LCD (GC9A01)

typedef enum { FACE_IDLE, FACE_THINKING, FACE_WAITING, FACE_DONE, FACE_ERROR, FACE_STATE_COUNT } face_state_t;

void face_init(uint32_t seed);
void face_look(float x, float y);  // -1..1, where the cursor is relative to screen center
void face_set_state(face_state_t s);
void face_update(uint32_t now_ms);
void face_render(uint16_t *fb);    // fills FACE_SIZE*FACE_SIZE RGB565 pixels
