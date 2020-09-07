#
# b startup
#
    .text
    .global _start

_start:
    call dopinit
main:
    leal __prog, %ecx
    jmp *(%ecx)

    .local __prog
__prog:
    .int ENTER, 0
    .int PSHSYM, _main
    .int CALL
    .int POP
    .int PSHCON, 0
    .int PSHSYM, _exit
    .int CALL


dopinit:
    mov $p0, %edi
    mov $pn, %esi
1:
    cmp %esi, %edi
    jge 2f
    mov (%edi), %eax
    shrl $2, (%eax)
    add $4, %edi
    jmp 1b
2:
    ret

    .section .vinit

