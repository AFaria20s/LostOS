#include "drivers/keyboard_layouts.h"
#include "lib/kstring.h"

/*
 * PS/2 set 1 scancodes. Add a layout by creating three KEYMAPs and adding
 * one entry to layouts[]. Unmapped keys stay zero and are ignored.
 */
#define KEYMAP(...) { __VA_ARGS__ }

enum {
  SC_ESC       = 0x01,
  SC_1         = 0x02,
  SC_2         = 0x03,
  SC_3         = 0x04,
  SC_4         = 0x05,
  SC_5         = 0x06,
  SC_6         = 0x07,
  SC_7         = 0x08,
  SC_8         = 0x09,
  SC_9         = 0x0A,
  SC_0         = 0x0B,
  SC_MINUS     = 0x0C,
  SC_EQUALS    = 0x0D,
  SC_BACKSPACE = 0x0E,
  SC_TAB       = 0x0F,
  SC_Q         = 0x10,
  SC_W         = 0x11,
  SC_E         = 0x12,
  SC_R         = 0x13,
  SC_T         = 0x14,
  SC_Y         = 0x15,
  SC_U         = 0x16,
  SC_I         = 0x17,
  SC_O         = 0x18,
  SC_P         = 0x19,
  SC_LBRACKET  = 0x1A,
  SC_RBRACKET  = 0x1B,
  SC_ENTER     = 0x1C,
  SC_A         = 0x1E,
  SC_S         = 0x1F,
  SC_D         = 0x20,
  SC_F         = 0x21,
  SC_G         = 0x22,
  SC_H         = 0x23,
  SC_J         = 0x24,
  SC_K         = 0x25,
  SC_L         = 0x26,
  SC_SEMICOLON = 0x27,
  SC_APOSTROPHE = 0x28,
  SC_GRAVE     = 0x29,
  SC_BACKSLASH = 0x2B,
  SC_Z         = 0x2C,
  SC_X         = 0x2D,
  SC_C         = 0x2E,
  SC_V         = 0x2F,
  SC_B         = 0x30,
  SC_N         = 0x31,
  SC_M         = 0x32,
  SC_COMMA     = 0x33,
  SC_DOT       = 0x34,
  SC_SLASH     = 0x35,
  SC_KP_STAR   = 0x37,
  SC_SPACE     = 0x39,
  SC_102ND     = 0x56
};

enum {
  CP437_C_CEDILLA_LOWER = (char)0x87,
  CP437_C_CEDILLA_UPPER = (char)0x80,
  CP437_FEM_ORDINAL     = (char)0xA6,
  CP437_MASC_ORDINAL    = (char)0xA7,
  CP437_SECTION         = (char)0x15,
  CP437_LEFT_GUILLEMET  = (char)0xAE,
  CP437_RIGHT_GUILLEMET = (char)0xAF,
  CP437_POUND           = (char)0x9C
};

static const char us_normal[KEYBOARD_LAYOUT_SCANCODE_COUNT] = KEYMAP(
  [SC_ESC] = 27, [SC_1] = '1', [SC_2] = '2', [SC_3] = '3',
  [SC_4] = '4', [SC_5] = '5', [SC_6] = '6', [SC_7] = '7',
  [SC_8] = '8', [SC_9] = '9', [SC_0] = '0', [SC_MINUS] = '-',
  [SC_EQUALS] = '=', [SC_BACKSPACE] = '\b', [SC_TAB] = '\t',
  [SC_Q] = 'q', [SC_W] = 'w', [SC_E] = 'e', [SC_R] = 'r',
  [SC_T] = 't', [SC_Y] = 'y', [SC_U] = 'u', [SC_I] = 'i',
  [SC_O] = 'o', [SC_P] = 'p', [SC_LBRACKET] = '[',
  [SC_RBRACKET] = ']', [SC_ENTER] = '\n', [SC_A] = 'a',
  [SC_S] = 's', [SC_D] = 'd', [SC_F] = 'f', [SC_G] = 'g',
  [SC_H] = 'h', [SC_J] = 'j', [SC_K] = 'k', [SC_L] = 'l',
  [SC_SEMICOLON] = ';', [SC_APOSTROPHE] = '\'', [SC_GRAVE] = '`',
  [SC_BACKSLASH] = '\\', [SC_Z] = 'z', [SC_X] = 'x', [SC_C] = 'c',
  [SC_V] = 'v', [SC_B] = 'b', [SC_N] = 'n', [SC_M] = 'm',
  [SC_COMMA] = ',', [SC_DOT] = '.', [SC_SLASH] = '/',
  [SC_KP_STAR] = '*', [SC_SPACE] = ' '
);

static const char us_shift[KEYBOARD_LAYOUT_SCANCODE_COUNT] = KEYMAP(
  [SC_ESC] = 27, [SC_1] = '!', [SC_2] = '@', [SC_3] = '#',
  [SC_4] = '$', [SC_5] = '%', [SC_6] = '^', [SC_7] = '&',
  [SC_8] = '*', [SC_9] = '(', [SC_0] = ')', [SC_MINUS] = '_',
  [SC_EQUALS] = '+', [SC_BACKSPACE] = '\b', [SC_TAB] = '\t',
  [SC_Q] = 'Q', [SC_W] = 'W', [SC_E] = 'E', [SC_R] = 'R',
  [SC_T] = 'T', [SC_Y] = 'Y', [SC_U] = 'U', [SC_I] = 'I',
  [SC_O] = 'O', [SC_P] = 'P', [SC_LBRACKET] = '{',
  [SC_RBRACKET] = '}', [SC_ENTER] = '\n', [SC_A] = 'A',
  [SC_S] = 'S', [SC_D] = 'D', [SC_F] = 'F', [SC_G] = 'G',
  [SC_H] = 'H', [SC_J] = 'J', [SC_K] = 'K', [SC_L] = 'L',
  [SC_SEMICOLON] = ':', [SC_APOSTROPHE] = '"', [SC_GRAVE] = '~',
  [SC_BACKSLASH] = '|', [SC_Z] = 'Z', [SC_X] = 'X', [SC_C] = 'C',
  [SC_V] = 'V', [SC_B] = 'B', [SC_N] = 'N', [SC_M] = 'M',
  [SC_COMMA] = '<', [SC_DOT] = '>', [SC_SLASH] = '?',
  [SC_KP_STAR] = '*', [SC_SPACE] = ' '
);

static const char empty_altgr[KEYBOARD_LAYOUT_SCANCODE_COUNT] = KEYMAP();

static const char pt_normal[KEYBOARD_LAYOUT_SCANCODE_COUNT] = KEYMAP(
  [SC_ESC] = 27, [SC_1] = '1', [SC_2] = '2', [SC_3] = '3',
  [SC_4] = '4', [SC_5] = '5', [SC_6] = '6', [SC_7] = '7',
  [SC_8] = '8', [SC_9] = '9', [SC_0] = '0', [SC_MINUS] = '\'',
  [SC_EQUALS] = CP437_LEFT_GUILLEMET, [SC_BACKSPACE] = '\b',
  [SC_TAB] = '\t', [SC_Q] = 'q', [SC_W] = 'w', [SC_E] = 'e',
  [SC_R] = 'r', [SC_T] = 't', [SC_Y] = 'y', [SC_U] = 'u',
  [SC_I] = 'i', [SC_O] = 'o', [SC_P] = 'p', [SC_LBRACKET] = '+',
  [SC_RBRACKET] = '`', [SC_ENTER] = '\n', [SC_A] = 'a',
  [SC_S] = 's', [SC_D] = 'd', [SC_F] = 'f', [SC_G] = 'g',
  [SC_H] = 'h', [SC_J] = 'j', [SC_K] = 'k', [SC_L] = 'l',
  [SC_SEMICOLON] = CP437_C_CEDILLA_LOWER, [SC_APOSTROPHE] = CP437_MASC_ORDINAL,
  [SC_GRAVE] = '\\', [SC_BACKSLASH] = '~', [SC_Z] = 'z', [SC_X] = 'x',
  [SC_C] = 'c', [SC_V] = 'v', [SC_B] = 'b', [SC_N] = 'n',
  [SC_M] = 'm', [SC_COMMA] = ',', [SC_DOT] = '.', [SC_SLASH] = '-',
  [SC_KP_STAR] = '*', [SC_SPACE] = ' ', [SC_102ND] = '<'
);

static const char pt_shift[KEYBOARD_LAYOUT_SCANCODE_COUNT] = KEYMAP(
  [SC_ESC] = 27, [SC_1] = '!', [SC_2] = '"', [SC_3] = '#',
  [SC_4] = '$', [SC_5] = '%', [SC_6] = '&', [SC_7] = '/',
  [SC_8] = '(', [SC_9] = ')', [SC_0] = '=', [SC_MINUS] = '?',
  [SC_EQUALS] = CP437_RIGHT_GUILLEMET, [SC_BACKSPACE] = '\b',
  [SC_TAB] = '\t', [SC_Q] = 'Q', [SC_W] = 'W', [SC_E] = 'E',
  [SC_R] = 'R', [SC_T] = 'T', [SC_Y] = 'Y', [SC_U] = 'U',
  [SC_I] = 'I', [SC_O] = 'O', [SC_P] = 'P', [SC_LBRACKET] = '*',
  [SC_RBRACKET] = '^', [SC_ENTER] = '\n', [SC_A] = 'A',
  [SC_S] = 'S', [SC_D] = 'D', [SC_F] = 'F', [SC_G] = 'G',
  [SC_H] = 'H', [SC_J] = 'J', [SC_K] = 'K', [SC_L] = 'L',
  [SC_SEMICOLON] = CP437_C_CEDILLA_UPPER, [SC_APOSTROPHE] = CP437_FEM_ORDINAL,
  [SC_GRAVE] = '|', [SC_BACKSLASH] = '^', [SC_Z] = 'Z', [SC_X] = 'X',
  [SC_C] = 'C', [SC_V] = 'V', [SC_B] = 'B', [SC_N] = 'N',
  [SC_M] = 'M', [SC_COMMA] = ';', [SC_DOT] = ':', [SC_SLASH] = '_',
  [SC_KP_STAR] = '*', [SC_SPACE] = ' ', [SC_102ND] = '>'
);

static const char pt_altgr[KEYBOARD_LAYOUT_SCANCODE_COUNT] = KEYMAP(
  [SC_1] = '|', [SC_2] = '@', [SC_3] = CP437_POUND,
  [SC_4] = CP437_SECTION, [SC_7] = '{', [SC_8] = '[',
  [SC_9] = ']', [SC_0] = '}', [SC_GRAVE] = '\\'
);

const struct layout layouts[] = {
  {"default", pt_normal, pt_shift, pt_altgr, KEYBOARD_LAYOUT_SCANCODE_COUNT},
  {"pt", pt_normal, pt_shift, pt_altgr, KEYBOARD_LAYOUT_SCANCODE_COUNT},
  {"us", us_normal, us_shift, empty_altgr, KEYBOARD_LAYOUT_SCANCODE_COUNT},
};

const size_t layouts_count = sizeof(layouts) / sizeof(layouts[0]);

const struct layout *layout_find_by_name(const char *name) {
  if (!name)
    return NULL;

  for (size_t i = 0; i < layouts_count; i++) {
    if (k_strcmp(name, layouts[i].name) == 0)
      return &layouts[i];
  }
  return NULL;
}
