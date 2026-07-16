#ifndef IDT_H
#define IDT_H

int idt_init(void);
int idt_is_ready(void);
int idt_keyboard_irq_is_ready(void);

#endif
