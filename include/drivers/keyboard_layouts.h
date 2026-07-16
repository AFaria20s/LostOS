#ifndef KEYBOARD_LAYOUTS_H
#define KEYBOARD_LAYOUTS_H

#include <stddef.h>

#define KEYBOARD_LAYOUT_SCANCODE_COUNT 128

struct layout {
  const char *name;
  const char *normal;
  const char *shift;
  const char *altgr;
  size_t size;
};

extern const struct layout layouts[];
extern const size_t layouts_count;

const struct layout *layout_find_by_name(const char *name);

#endif
