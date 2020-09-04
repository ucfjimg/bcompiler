#
# B string routines
#
    .text

    .global .char
.char:
    .int NCALL, char

    .local char
char:
    mov 8(%ebp), %esi       # string address
    mov 12(%ebp), %edx      # offset
    xor %eax, %eax
    movb (%esi,%edx), %al
    jmp *(%ecx)
