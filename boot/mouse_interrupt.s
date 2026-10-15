.section .text
.global mouse_wrapper

mouse_wrapper:
    pusha
    call mouse_handler
    popa
    iret
