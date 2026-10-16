#include "protocol.h"
#include "eye.h"
#include <stdlib.h>
#include <string.h>

static const char *STATE_NAMES[EYE_STATE_COUNT] = {"idle", "thinking", "done", "error"};

// Minimal parser for our flat messages: returns a pointer to the value after "key":
static const char *value_of(const char *line, const char *key) {
    size_t n = strlen(key);
    for (const char *p = strchr(line, '"'); p; p = strchr(p + 1, '"')) {
        if (strncmp(p + 1, key, n) == 0 && p[1 + n] == '"') {
            p += n + 2;
            while (*p == ' ' || *p == ':') p++;
            return p;
        }
    }
    return NULL;
}

static int is_string(const char *val, const char *s) {
    size_t n = strlen(s);
    return val && *val == '"' && strncmp(val + 1, s, n) == 0 && val[1 + n] == '"';
}

void protocol_handle_line(const char *line) {
    const char *t = value_of(line, "t");
    if (is_string(t, "look")) {
        const char *x = value_of(line, "x"), *y = value_of(line, "y");
        if (x && y) eye_look(strtof(x, NULL), strtof(y, NULL));
    } else if (is_string(t, "state")) {
        const char *v = value_of(line, "v");
        for (int i = 0; i < EYE_STATE_COUNT; i++)
            if (is_string(v, STATE_NAMES[i])) eye_set_state((eye_state_t)i);
    }
}
