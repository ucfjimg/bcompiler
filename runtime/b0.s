#
# b startup
#

SYSBRK = 45

    .text
    .global $start

$start:
    call meminit
    call argv
    call dopinit
main:
    leal __prog, %ecx
    jmp *(%ecx)

    .local __prog
__prog:
    .int ENTER, 0
    .int PSHSYM, _main
    .int DEREF
    .int CALL
    .int POP
    .int PSHCON, 0
    .int PSHSYM, _exit
    .int DEREF
    .int CALL

    .data
memtop:
    .int    0

    .text
meminit:
    mov $SYSBRK, %eax
    xor %ebx, %ebx
    int $0x80
    mov %eax, memtop
    ret

    .data
    .global _argv, envp
    .align 4
_argv:
    .int    0
envp:
    .int    0
    .text
#
# build argv
#
argv:
    cld
    leal 4(%esp), %ebp
#
# first, add up the length of all argv strings (rounded up to
# word boundaries)    
#
    movl (%ebp), %ecx       # number of args
    xor %eax, %eax          # length of args
1:    
    add $4, %ebp            # next arg 
    mov (%ebp), %edi        # edi -> arg
    push %eax
    xor %eax, %eax          # scan for nul
    push %ecx
    mov $-1, %ecx           # search forever
    repne scasb             # find nul
    pop %ecx
    sub (%ebp), %edi        # length of arg in %edi
    add $3, %edi            # round up to next word
    and $0xfffffffc, %edi
    pop %eax
    add %edi, %eax          # add into total
    loop 1b
 #
 # save off environment pointer
 #
    add $8, %ebp
    mov %ebp, envp

 #
 # adjust the top of memory
 #
    mov memtop, %ebx
    add $3, %ebx
    and $0xfffffffc, %ebx   # round up and
    push %ebx               # save pointer for argv
    add %eax, %ebx          # space for strings
    mov 8(%esp), %ecx       # number of arguments
    inc %ecx                # slot for arg count
    shl $2, %ecx            # convert to words
    add %ecx, %ebx          # how much we need total
    mov $45, %eax           # brk syscall
    int $0x80
    mov %eax, memtop
    pop %ebx                # get back base
    shr $2, %ebx
    mov %ebx, _argv         # save pointer 
    shl $2, %ebx
    leal (%ebx, %ecx), %edi # string space
    leal 4(%esp), %edx      # pointer to orig args
    movl (%edx), %ecx
    movl %ecx, (%ebx)       # arg count
1:
    add $4, %edx            # -> next string ptr
    add $4, %ebx            # -> next string loc
    shr $2, %edi            # string start to ptr
    mov %edi, (%ebx)        # save in vector
    shl $2, %edi
    mov (%edx), %esi        # start of string
2:
    lodsb                   # strcpy
    or %al, %al
    jz 3f
    stosb
    jmp 2b
3:
    mov $0xff, %al          # B string terminator
    stosb

    add $3, %edi            # pad for next string
    and $0xfffffffc, %edi
    loop 1b
    ret
#
# initialize addresses that need to be shifted into pointers
#
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


