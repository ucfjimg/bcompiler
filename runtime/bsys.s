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

putchar:
    push %ecx
    push %ebx
    push 12(%esp)

    mov $4, %eax        # write system
    mov $1, %ebx        # stdout
    mov %esp, %ecx      # buffer to write
    mov $1, %edx        # bytes to write
    int $0x80

    pop %eax
    pop %ebx
    pop %ecx
    push %eax
    jmp *(%ecx)

    .align 4
    .global _debug
_debug:
    .int NCALL, debug
    .int RET

debug:
    int $3
    push $0
    jmp *(%ecx)

