#
# B syscall wrappers
#
    .text

    .align 4
    .global _exit
_exit:
    .int NCALL, exit


    .local exit
exit:
    mov 4(%esp), %ebx   # return address
    mov $1, %eax        # exit syscall
    int $0x80
    
    .align 4
    .global _putchar
_putchar:
    .int NCALL, putchar
    .int RET

    .local putchar
outch:
    .byte 0

putchar:
    push %ecx
    push %ebx
    mov 4(%esp), %eax
    mov %al, outch

    mov $4, %eax        # write system
    mov $1, %ebx        # stdout
    leal outch, %ecx    # buffer to write
    mov 1, %edx         # bytes to write
    int $0x80

    pop %ebx
    pop %ecx
    push %eax
    jmp *(%ecx)
