#include "fs/config.h"
#include "fs/vfs.h"
#include "lib/kstring.h"

static struct config_entry entries[CONFIG_MAX_ENTRIES];
static int entry_count = 0;

#define CONFIG_PATH "/etc/lost.cfg"

static void config_parse_line(char *line) {
    if (entry_count >= CONFIG_MAX_ENTRIES)
        return;

    // find the '='
    int eq_pos = -1;
    for (int i = 0; line[i]; i++) {
        if (line[i] == '=') {
            eq_pos = i;
            break;
        }
    }

    if (eq_pos < 0)
        return; // invalid line, no '='

    line[eq_pos] = '\0';
    const char *key = line;
    const char *value = line + eq_pos + 1;

    k_strcp(entries[entry_count].key, key);
    k_strcp(entries[entry_count].value, value);
    entry_count++;
}

static int config_load(void) {
    struct vfs_file file;
    char buf[512];
    uint32_t bytes_read;
    char line_buf[96];
    int line_len = 0;

    entry_count = 0;

    if (!vfs_open(CONFIG_PATH, &file))
        return 0;

    while ((bytes_read = vfs_read(&file, buf, sizeof(buf))) > 0) {
        for (uint32_t i = 0; i < bytes_read; i++) {
            if (buf[i] == '\n') {
                line_buf[line_len] = '\0';
                if (line_len > 0)
                    config_parse_line(line_buf);
                line_len = 0;
            } else if (line_len < 95) {
                line_buf[line_len++] = buf[i];
            }
        }
    }

    if (line_len > 0) {
        line_buf[line_len] = '\0';
        config_parse_line(line_buf);
    }

    return 1;
}

static void config_write_defaults(void) {
    struct vfs_file file;
    const char *defaults =
        "username=$2lost\n"
        "hostname=$alostos\n"
        "prompt=$u@$h:$p$ "
        "theme=default\n";

    vfs_create(CONFIG_PATH);
    if (vfs_open(CONFIG_PATH, &file))
        vfs_write(&file, defaults, k_strlen(defaults));
}

int config_init(void) {
    struct vfs_file test;

    if (!vfs_open(CONFIG_PATH, &test))
        config_write_defaults();

    return config_load();
}

int config_reload(void) {
    return config_load();
}

const char *config_get(const char *key) {
    for (int i = 0; i < entry_count; i++) {
        if (k_strcmp(entries[i].key, key) == 0)
            return entries[i].value;
    }
    return NULL;
}

int config_set(const char *key, const char *value) {
    // update in memory first
    for (int i = 0; i < entry_count; i++) {
        if (k_strcmp(entries[i].key, key) == 0) {
            k_strcp(entries[i].value, value);
            goto persist;
        }
    }

    // not found, add new entry
    if (entry_count >= CONFIG_MAX_ENTRIES)
        return 0;

    k_strcp(entries[entry_count].key, key);
    k_strcp(entries[entry_count].value, value);
    entry_count++;

    persist:
    // rewrite the whole file with current entries
    {
        struct vfs_file file;
        char line[CONFIG_KEY_LEN + CONFIG_VALUE_LEN + 2];

        vfs_create(CONFIG_PATH); // no-op if it already exists, depends on your vfs_create
        if (!vfs_open(CONFIG_PATH, &file))
            return 0;

        for (int i = 0; i < entry_count; i++) {
            k_strcat(line, entries[i].key, entries[i].value, "=");
            k_strapp(line, "\n");
            vfs_write(&file, line, k_strlen(line));
        }
    }

    return 1;
}