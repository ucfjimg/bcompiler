#
# b startup
#
    .global _start
_start:
    
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
