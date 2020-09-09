#
# B syscall wrappers
#

# int80 Linux syscalls for x86. These numbers are burned in to stone and cannot change.
SYSEXIT=1
SYSFORK=2
SYSREAD=3
SYSWRIT=4
SYSBRK=45
SYSOPEN=5
SYSCLOS=6

# standard descriptors
STDIN=0
STDOUT=1
STDERR=2

# file open flags
RDONLY=0
WRONLY=1

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

    .align 4
    .global _open
_open:
    .int .+4
0:
    .pushsection .pinit, "aw", @progbits
    .int 0b-4
    .popsection
    .int NCALL, open 
    .int RET

open:
    push %ecx
    push %ebx
    mov $SYSOPEN, %eax
    mov 12(%esp), %ebx
    shl $2, %ebx
    mov 16(%esp), %ecx
    or %ecx, %ecx           # NB 0 = RDONLY
    jz 1f
    mov $WRONLY, %ecx        
1:
    xor %edx, %edx
    call scstr
    int $0x80
    movb $0xff, (%edi)
    pop %ebx
    pop %ecx
    push %eax
    jmp *(%ecx)

    .align 4
    .global _close
_close:
    .int .+4
0:
    .pushsection .pinit, "aw", @progbits
    .int 0b-4
    .popsection
    .int NCALL, close 
    .int RET

close:
    push %ebx
    mov $SYSCLOS, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    push %eax
    jmp *(%ecx)

    .align 4
    .global _read
_read:
    .int .+4
0:
    .pushsection .pinit, "aw", @progbits
    .int 0b-4
    .popsection
    .int NCALL, read 
    .int RET

read:
    push %ecx
    push %ebx
    mov $SYSREAD, %eax
    mov 12(%esp), %ebx
    mov 16(%esp), %ecx
    shl $2, %ecx
    mov 20(%esp), %edx
    int $0x80
    pop %ebx
    pop %ecx
    push %eax
    jmp *(%ecx)

#
# Convert B strings to nul-terminated strings (temporarily) for 
# system calls. Takes the original pointer in ebx; returns
# with the string in place nul terminated and edi pointing to
# where to replace the *e when done.
#
    .local scstr
scstr:
    push %ecx           # save regs
    push %eax
    mov %ebx, %edi
    mov $-1, %ecx       # scan count (all of memory)
    cld
    mov $0xff, %al      # *e character
    repne scasb         # esi -> just past it
    dec %edi            # -> nul
    movb $0, (%edi)
    pop %eax
    pop %ecx
    ret


