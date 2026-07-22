#include "lib/path.h"
#include "lib/kstring.h"

static void path_parent(char *path) {
    int length = k_strlen(path);

    while (length > 1 && path[length - 1] != '/')
        length--;

    path[length] = '\0';
}

static void path_append(char *out, const char *path) {
    const char *part = path;

    while (*part) {
        const char *start;
        int length = 0;
        int out_length;

        while (*part == '/')
            part++;

        if (!*part)
            break;

        start = part;
        while (*part && *part != '/') {
            part++;
            length++;
        }

        if (length == 1 && start[0] == '.')
            continue;

        if (length == 2 && start[0] == '.' && start[1] == '.') {
            path_parent(out);
            continue;
        }

        out_length = k_strlen(out);
        if (out_length > 1)
            out[out_length++] = '/';

        for (int i = 0; i < length; i++)
            out[out_length + i] = start[i];
        out[out_length + length] = '\0';
    }
}

void resolve_path(const char *cwd, const char *input, char *out) {
    out[0] = '/';
    out[1] = '\0';

    if (input[0] != '/')
        path_append(out, cwd);

    path_append(out, input);
}
