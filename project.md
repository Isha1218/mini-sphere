# Project: Mini Sphere

## Overview
An open-source desk companion modeled on the Las Vegas Sphere (https://github.com/Isha1218/mini-sphere): a minimal glowing face that follows your mouse and shows what your coding agents are doing (thinking, done, error). Software first, in a browser simulator; hardware (ESP32-S3 + 240×240 round GC9A01 LCD under a glass dome) later.

## Tech Stack
- **firmware/**: portable C99 renderer and protocol parser. Compiled to wasm (Emscripten) for the sim now, and for the ESP32 later. No platform deps, so it carries over to hardware.
- **daemon/**: Python (`websockets`, `pyobjc-framework-Quartz` for the cursor on macOS).
- **sim/**: one HTML page that runs the wasm renderer on a canvas.

## Architecture
```
Claude Code hooks ─► minisphere send <state> ─┐
cursor (30 Hz) ───────────────────────────┼─► daemon hub (ws://localhost:8765) ─► sim (wasm)   [later: USB serial ─► ESP32]
```
- Protocol: one JSON object per line. `{"t":"look","x":-1..1,"y":-1..1}`, `{"t":"state","v":"idle|thinking|done|error"}`.
- The sphere does all drawing; the computer only sends messages. Frames are never streamed.
- Hardware port = new display/serial glue around the same `eye.c` + `protocol.c`, plus a serial transport in the daemon.

## Key Files
- `firmware/eye.c`: the face. Glowing white eyes on a black glass ball that turn toward the cursor; idle = smiling eyes, thinking = uneven lids scanning, done = squeezed-shut ^ ^, error = slanted lids + shake. Expressions morph; blinks squash; glances around when the cursor is still
- `firmware/protocol.c`: line parser → `eye_look` / `eye_set_state`
- `sim/index.html`, `sim/build.sh`: browser simulator and wasm build
- `daemon/sphere/hub.py`: WebSocket relay, cursor tracking, serves the sim on :8000
- `daemon/sphere/cli.py`: `minisphere run`, `minisphere send <state>`

## How to Run
```sh
brew install emscripten
python3 -m venv .venv && .venv/bin/pip install -e .
sim/build.sh                 # rebuild after editing firmware/
.venv/bin/minisphere run         # then open http://localhost:8000
.venv/bin/minisphere send done   # idle | thinking | done | error
```

## Status
- Stage 1 (done): sim + daemon + mouse tracking + `minisphere send`.
- Next: wire Claude Code hooks; Stage 2: three.js 3D preview (dome, tilt, viewing angle).
