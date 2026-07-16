#ifndef COMMANDS_H
#define COMMANDS_H

// Runs shell line
void cmd_execute(char *line);
int cmd_autocomplete(const char *prefix, void (*putc)(char));

#endif
