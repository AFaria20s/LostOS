#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_MAX_ENTRIES 16
#define CONFIG_KEY_LEN 32
#define CONFIG_VALUE_LEN 64

struct config_entry {
    char key[CONFIG_KEY_LEN];
    char value[CONFIG_VALUE_LEN];
};

// load config from /etc/lost.cfg into memory then
// creates the file with defaults if it does not exist
int config_init(void);

// reload config from disk
int config_reload(void);

// get a config value by key, returns NULL if not found
const char *config_get(const char *key);

// set a config value in memory and persist it to disk
int config_set(const char *key, const char *value);

#endif