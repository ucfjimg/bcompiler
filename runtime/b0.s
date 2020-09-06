#
# b startup
#
    .text
    .global _start

_start:
#    call dovinit    
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


dovinit:
    leal v0, %edi
    leal vn, %esi
1:
    cmp %esi, %edi
    jge 2f
    shrl (%edi)
    add $4, %edi
2:
    ret

    .section .vinit
    .global v0
v0:

