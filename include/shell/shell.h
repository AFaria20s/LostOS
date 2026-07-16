#ifndef SHELL_H
#define SHELL_H

// Show prompt and position cursor
void shell_prompt(void);

// Process current input line and run command
void shell_process(void);
// Handle a key event
void shell_input(int key);

// Print command history
void shell_print_history(void);

#endif
