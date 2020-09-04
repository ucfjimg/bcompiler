#
# B syscall wrappers
#
    .text

    .global .exit
.exit:
    .int NCALL, exit


    .local exit
exit:
    mov 8(%ebp), %ebx   # return address
    mov $1, %eax        # exit syscall
    int $0x80
    
    .global .putchar
.putchar:
    .int NCALL, putchar

    .local putchar
outch:
    .byte 0

putchar:
    push %ecx
    push %ebx
    mov 8(%ebp), %eax
    mov %al, outch

    mov $4, %eax        # write system
    mov $1, %ebx        # stdout
    leal outch, %ecx    # buffer to write
    mov 1, %edx         # bytes to write
    int $0x80

    pop %ebx
    pop %ecx
    jmp *(%ecx)
