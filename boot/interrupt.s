.section .text
.global keyboard_wrapper
keyboard_wrapper:
    pusha
    call keyboard_handler
    popa
    iret