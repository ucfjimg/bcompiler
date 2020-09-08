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
    mov 8(%esp), %edx       # offset
    xor %eax, %eax
    shl $2,%esi
    movb (%edx,%esi), %al
    push %eax
    jmp *(%ecx)

    .align 4
    .global _lchar
_lchar:
    .int .+4
0:
    .pushsection .pinit, "aw", @progbits
    .int 0b-4
    .popsection
    .int NCALL, __lchar
    .int RET

    .local __lchar
__lchar:
    mov 4(%esp), %esi       # string address
    mov 8(%esp), %edx       # offset
    mov 12(%esp), %eax      # character to store
    shl $2,%esi
    movb %al, (%edx,%esi) 
    push %eax
    jmp *(%ecx)
