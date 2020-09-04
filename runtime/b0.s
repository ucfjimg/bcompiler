#
# b startup
#
    .global _start
_start:
    
    leal .main, %ecx
    pushl $exit
    jmp *(%ecx)

exit:
    .int PSHCON, 0
    .int .exit
    .int CALL

