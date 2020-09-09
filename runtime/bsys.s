#
# B syscall wrappers
#

# int80 Linux syscalls for x86. These numbers are burned in to stone and cannot change.
SYSEXIT=1
SYSFORK=2
SYSREAD=3
SYSWRIT=4
SYSBRK=45

# standard descriptors
STDIN=0
STDOUT=1
STDERR=2

    .text

    .align 4
    .global _exit
_exit:
    .int .+4
0:
    .pushsection .pinit, "aw", @progbits
    .int 0b-4
    .popsection
    .int NCALL, exit


    .local exit
exit:
    mov 4(%esp), %ebx   # return address
    mov $SYSEXIT, %eax  # exit syscall
    int $0x80
    
    .align 4
    .global _putchar
_putchar:
    .int .+4
0:
    .pushsection .pinit, "aw", @progbits
    .int 0b-4
    .popsection
    .int NCALL, putchar
    .int RET

    .local putchar

putchar:
    push %ecx
    push %ebx
    push 12(%esp)

    mov $SYSWRIT, %eax  # write system
    mov $STDOUT, %ebx   # stdout
    mov %esp, %ecx      # buffer to write
    mov $1, %edx        # bytes to write
    int $0x80

    pop %eax
    pop %ebx
    pop %ecx
    push %eax
    jmp *(%ecx)

    .align 4
    .global _getchar
_getchar:
    .int .+4
0:
    .pushsection .pinit, "aw", @progbits
    .int 0b-4
    .popsection
    .int NCALL, getchar
    .int RET

    .local getchar

getchar:
    push %ecx
    push %ebx
    push $0

    mov $SYSREAD, %eax  # write system
    mov $STDOUT, %ebx   # stdout
    mov %esp, %ecx      # buffer to read
    mov $1, %edx        # bytes to read
    int $0x80
    cmp $1, %al         # should get a byte, eventually
    je 1f
    movb $0xff, (%esp)  # return EOF otherwise
1:
    pop %eax
    pop %ebx
    pop %ecx
    push %eax
    jmp *(%ecx)


    .align 4
    .global _brk
_brk:
    .int .+4
0:
    .pushsection .pinit, "aw", @progbits
    .int 0b-4
    .popsection
    .int NCALL, brk
    .int RET

brk:
    push %ecx
    push %ebx
    mov 12(%esp), %ebx

    mov $SYSBRK, %eax
    int $0x80
    
    pop %ecx
    pop %ebx
    push %eax
    jmp *(%ecx)


