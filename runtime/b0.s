#
# b startup
#
    .global _start
_start:
    
    movl .main, %ecx
    pushl $exit
    jmp *(%ecx)

exit:
    .int PSHCON, 0
    .int .exit
    .int CALL

