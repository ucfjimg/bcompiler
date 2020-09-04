#
# b startup
#
    .global _start
_start:
    
    leal _main, %ecx
    pushl $exit
    jmp *(%ecx)

exit:
    .int PSHCON, 0
    .int _exit
    .int CALL

