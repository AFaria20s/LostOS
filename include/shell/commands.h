#ifndef COMMANDS_H
#define COMMANDS_H

// Runs shell line
void commands_execute(char *line);
int commands_input_line(const char *line);
void commands_input_eof(void);
int commands_input_active(void);
int autocomplete_input(const char *input, void (*putc)(char));

#endif
