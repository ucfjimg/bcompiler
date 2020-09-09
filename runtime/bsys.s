#
# B syscall wrappers
#

# int80 Linux syscalls for x86. These numbers are burned in to stone and cannot change.
SYSEXIT=1
SYSFORK=2
SYSREAD=3
SYSWRITE=4
SYSOPEN=5
SYSCLOSE=6
SYSWAIT=7
SYSCREAT=8
SYSCHDIR=12
SYSCHMOD=15
SYSCHOWN=16
SYSBRK=45

# standard descriptors
STDIN=0
STDOUT=1
STDERR=2

# file open flags
RDONLY=0
WRONLY=1


#
# mkncall - create a threaded interpreter stub for a native code function
#
    .macro mkncall name
    .align 4
    .global _\name
_\name :
    .int .+4
0:
    .pushsection .pinit, "aw", @progbits
    .int 0b-4
    .popsection
    .int NCALL, \name
    .int RET
    .local \name
\name :
    .endm
    
    .text

#
# exit(rc)
# exits the process with return code rc
#
    mkncall exit
    mov 4(%esp), %ebx   # return address
    mov $SYSEXIT, %eax  # exit syscall
    int $0x80
    
#
# putchar(ch)
# writes the character ch to the console
#
    mkncall putchar
    push %ecx
    push %ebx
    push 12(%esp)

    mov $SYSWRITE, %eax # write syscal write syscall 
    mov $STDOUT, %ebx   # stdout
    mov %esp, %ecx      # buffer to write
    mov $1, %edx        # bytes to write
    int $0x80

    pop %eax
    pop %ebx
    pop %ecx
    push %eax
    jmp *(%ecx)

#
# getchar() 
# reads a character from the console and returns it, or '*e' on end of
# file or error
#
    mkncall getchar
    push %ecx
    push %ebx
    push $0

    mov $SYSREAD, %eax  # read syscall 
    mov $STDOUT, %ebx   # stdout
    mov %esp, %ecx      # buffer to read
    mov $1, %edx        # bytes to read
    int $0x80
    cmp $1, %al         # should get a byte, eventually
    je 1f
    movb $0xff, (%esp)  # return EOF otherwise
1:
    pop %eax
    pop %ebx
    pop %ecx
    push %eax
    jmp *(%ecx)


#
# brk(fence)     NOT IN ORIGINAL STDLIB
# sets the memory fence to address 'fence', if possible; returns the
# new fence or a negative number on error.
#
    mkncall brk
    push %ecx
    push %ebx
    mov 12(%esp), %ebx

    mov $SYSBRK, %eax
    int $0x80
    
    pop %ecx
    pop %ebx
    push %eax
    jmp *(%ecx)

#
# open(fname, rdwr)
# opens an existing file for read (if rdwr == 0) or write
# (if rdwr != 0). returns the file descriptor, or a negative
# number on error.
#
    mkncall open
    push %ecx
    push %ebx
    mov $SYSOPEN, %eax
    mov 12(%esp), %ebx
    shl $2, %ebx
    mov 16(%esp), %ecx
    or %ecx, %ecx           # NB 0 = RDONLY
    jz 1f
    mov $WRONLY, %ecx        
1:
    xor %edx, %edx
    call scstr
    int $0x80
    movb $0xff, (%edi)
    pop %ebx
    pop %ecx
    push %eax
    jmp *(%ecx)

#
# creat(fname, mode)
# creates a new file an opens in for write. if the file exists, it is truncated.
# 'mode' specifies the mode bits. returns a file descriptor on success or 
# a negative number on error.
#
    mkncall creat
    push %ecx
    push %ebx
    mov $SYSCREAT, %eax
    mov 12(%esp), %ebx
    shl $2, %ebx
    call scstr
    mov 16(%esp), %ecx
    int $0x80
    pop %ebx
    pop %ecx
    push %eax
    jmp *(%ecx)

#
# close(fd)
# closes the given file descriptor. returns on success or a negative
# number on failure.
#
    mkncall close
    push %ebx
    mov $SYSCLOSE, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    push %eax
    jmp *(%ecx)

#
# read(fd, buffer,size)
# read 'size' bytes from file descriptor 'fd' into vector 'buffer'. returns
# the number of bytes read, or a negative number on error.
#
    mkncall read
    push %ecx
    push %ebx
    mov $SYSREAD, %eax
    mov 12(%esp), %ebx
    mov 16(%esp), %ecx
    shl $2, %ecx
    mov 20(%esp), %edx
    int $0x80
    pop %ebx
    pop %ecx
    push %eax
    jmp *(%ecx)

#
# write(fd, buffer,size)
# write 'size' bytes to file descriptor 'fd' from vector 'buffer'. returns
# the number of bytes written, or a negative number on error.
#
    mkncall write
    push %ecx
    push %ebx
    mov $SYSWRITE, %eax
    mov 12(%esp), %ebx
    mov 16(%esp), %ecx
    shl $2, %ecx
    mov 20(%esp), %edx
    int $0x80
    pop %ebx
    pop %ecx
    push %eax
    jmp *(%ecx)

#
# chdir(dir)
# change the process current directory to 'dir'. Returns 0 on success
# or a negative number on error.
#
    mkncall chdir
    push %ebx
    mov $SYSCHDIR, %eax
    mov 8(%esp), %ebx
    shl $2, %ebx
    call scstr
    int $0x80
    pop %ebx
    push %eax
    jmp *(%ecx)

#
# chmod(path, mode)
# change the mode bits for the given inode. returns 0 on success or a
# negative numer on failure
#
    mkncall chmod
    push %ebx
    push %ecx
    mov $SYSCHMOD, %eax
    mov 12(%esp), %ebx
    mov 16(%esp), %ecx
    shl $2, %ebx
    call scstr
    int $0x80
    pop %ecx
    pop %ebx
    push %eax
    jmp *(%ecx)

#
# chown(path, onwer)
# change the owner of the given inode to 'owner', which is a uid.
# return negative on failure.
#
    mkncall chown
    push %ebx
    push %ecx
    mov $SYSCHOWN, %eax
    mov 12(%esp), %ebx
    mov 16(%esp), %ecx
    shl $2, %ebx
    call scstr
    mov $-1, %edx       # lchown expects a gid_t in EDX; -1 
                        # means ignore it.
    int $0x80
    pop %ecx
    pop %ebx
    push %eax
    jmp *(%ecx)

#
# fork()
# split the process in two. in the original (parent) process, returns
# the pid of the child, or a negative number on failure. on success,
# returns 0 in the new child process.
#
    mkncall fork
    mov $SYSFORK, %eax
    int $0x80
    push %eax
    jmp *(%ecx)

#
# wait()
# wait for any child process to exit and return that child's pid.
# returns -1 on error.
#
    mkncall wait
    push %ecx
    push %ebx
    mov $SYSWAIT, %eax
    mov $-1, %ebx       # -1 - wait for any child
    xor %ecx, %ecx      # NULL for status pointer
    xor %edx, %edx      # no option flags
    int $0x80
    pop %ebx
    pop %ecx
    push %eax
    jmp *(%ecx)

#
# Convert B strings to nul-terminated strings (temporarily) for 
# system calls. Takes the original pointer in ebx; returns
# with the string in place nul terminated and edi pointing to
# where to replace the *e when done.
#
    .local scstr
scstr:
    push %ecx           # save regs
    push %eax
    mov %ebx, %edi
    mov $-1, %ecx       # scan count (all of memory)
    cld
    mov $0xff, %al      # *e character
    repne scasb         # esi -> just past it
    dec %edi            # -> nul
    movb $0, (%edi)
    pop %eax
    pop %ecx
    ret


