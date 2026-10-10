.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

# Multiboot header and checksum required by bootloader

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 16
stack_bottom:
.skip 65536
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    // Initialize stack and call kernel
    mov $stack_top, %esp
    push %ebx
    push %eax
    call kernel_main
    add $8, %esp
    cli
1:  hlt
    jmp 1b
