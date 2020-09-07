#
# B string routines
#
    .text

    .align 4
    .global _char
_char:
    .int .+4
0:
    .pushsection .pinit, "aw", @progbits
    .int 0b-4
    .popsection
    .int NCALL, __char
    .int RET

    .local __char
__char:
    mov 4(%esp), %esi       # string address
    mov 8(%esp), %edx      # offset
    xor %eax, %eax
    movb (%esi,%edx), %al
    push %eax
    jmp *(%ecx)
