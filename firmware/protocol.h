// Wire protocol: one JSON object per line, e.g.
//   {"t":"look","x":0.3,"y":-0.2}
//   {"t":"state","v":"done"}       (idle | thinking | done | error)
#pragma once

void protocol_handle_line(const char *line);
