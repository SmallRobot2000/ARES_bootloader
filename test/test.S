.section .text
.global _start

_start:

    li a7, 1          # SBI_CONSOLE_PUTCHAR

    li a0, 'H'
    ecall

    li a0, 'e'
    ecall

    li a0, 'l'
    ecall

    li a0, 'l'
    ecall

    li a0, 'o'
    ecall

    li a0, '\n'
    ecall

hang:
    j hang
