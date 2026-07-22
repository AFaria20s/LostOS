#ifndef COMMANDS_H
#define COMMANDS_H

// Runs shell line
void commands_execute(char *line);
int autocomplete_input(const char *input, void (*putc)(char));

#endif
