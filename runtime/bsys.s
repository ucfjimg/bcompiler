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
SYSLINK=9
SYSUNLINK=10
SYSEXECVE=11
SYSCHDIR=12
SYSCHMOD=15
SYSCHOWN=16
SYSSEEK=19
SYSSETUID=23
SYSGETUID=24
SYSMKDIR=39
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
    mov $STDIN, %ebx    # stdin
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
    movb $0xff, (%edi)
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
# seek(fd, offset, pos)
# move the file pointer of fd. if pos = 0, offset is relative to the start
# of file. if pos = 1, it is relative to the current position. if pos = 2,
# it is relative to the end.
# 
# on success, returns the new position from the start of the file; a 
# negative number indicates an error.
#
    mkncall seek
    push %ecx
    push %edx
    mov $SYSSEEK, %eax
    mov 12(%esp), %ebx
    mov 16(%esp), %ecx
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
    movb $0xff, (%edi)
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
    movb $0xff, (%edi)
    pop %ecx
    pop %ebx
    push %eax
    jmp *(%ecx)

#
# chown(path, owner)
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
    movb $0xff, (%edi)
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
# getuid()
# return the uid of the current process
#
    mkncall getuid
    mov $SYSGETUID, %eax
    int $0x80
    push %eax
    jmp *(%ecx)

#
# link(oldpath, newpath)
# create a hard link newpath to oldpath. returns a negative number
# on error.
#
    mkncall link
    push %ebx
    push %ecx
    mov $SYSLINK, %eax
    mov 12(%esp), %ebx
    mov 16(%esp), %ecx
    shl $2, %ebx
    shl $2, %ecx
    call scstr
    push %edi
    push %ebx
    mov %ecx, %ebx
    call scstr
    pop %ebx
    int $0x80
    movb $0xff, (%edi)
    pop %edi
    movb $0xff, (%edi)
    pop %ecx
    pop %ebx
    push %eax
    jmp *(%ecx)

#
# unlink(path)
# remove the given link. returns a negative number on failure.
#
    mkncall unlink
    push %ebx
    mov $SYSUNLINK, %eax
    mov 8(%esp), %ebx
    shl $2, %ebx
    call scstr
    int $0x80
    movb $0xff, (%edi)
    pop %ebx
    push %eax
    jmp *(%ecx)

#
# mkdir(path, mode)
# create a new directory with permission bits 'mode'. returns a negative
# number on failure.
#
    mkncall mkdir
    push %ecx
    push %ebx
    mov $SYSMKDIR, %eax
    mov 12(%esp), %ebx
    shl $2, %ebx
    call scstr
    mov 16(%esp), %ecx
    int $0x80
    movb $0xff, (%edi)
    pop %ebx
    pop %ecx
    push %eax
    jmp *(%ecx)

#
# setuid(uid)
# sets the process uid to 'uid'. returns a negative number of failure.
#
    mkncall setuid 
    push %ebx
    mov $SYSSETUID, %eax
    mov 8(%esp), %ebx
    int $0x80
    pop %ebx
    push %eax
    jmp *(%ecx)


#
# execl(prog, arg1, arg2, ..., 0)
# replace the process with the program 'prog', with command line args
# arg1 arg2 ...
# never returns on success. returns a negative number on failure.
#
    mkncall execl
    push %ebp
    mov %esp, %ebp
    push %ecx
    push %ebx

    # walk the stack, converting the pointers to native addresses
    # and fixing end of string markers
    #
    push $0
    leal 8(%ebp), %esi
1:  mov (%esi), %ebx
    shl $2, %ebx
    or %ebx, %ebx
    jz 2f
    mov %ebx, (%esi)
    call scstr
    push %edi
    add $4, %esi
    jmp 1b
2:

    # now the args on the stack are set up as the syscall needs
    # unless the call fails, this won't return
    #
    mov $SYSEXECVE, %eax
    leal 8(%ebp), %ecx      # ecx -> arg array
    mov  (%ecx), %ebx       # ebx -> program
    mov envp, %edx          # environment
    int $0x80
    
    # NB we only get here if execve failed.
    # fix the strings back to B termination style
    #
1:  pop %edi
    or %edi, %edi
    jz 2f
    movb $0xff, (%edi)
    jmp 1b
2:
    pop %ebx
    pop %ecx
    pop %ebp
    push %eax
    jmp *(%ecx)

#
# execv(prog, args, count)
# like execl but the args are in a vector
#
    mkncall execv
    push %ebp
    mov %esp, %ebp
    push %ecx
    push %ebx

    movl 12(%ebp), %esi         # esi -> base of vector
    shl $2, %esi                # ptr to address
    movl 16(%ebp), %ecx         # # things in array
    shl $2, %ecx                # # bytes
    add %ecx, %esi              # esi -> end of vector
    shr $2, %ecx
    push $0                     # push terminator
1:  add $-4, %esi               # previous arg
    push (%esi)                 # push on stack
    shll $2, (%esp)             # ptr to address of string
    loop 1b                     # for all args
    push 8(%ebp)                # 0'th arg is the program name
    shll $2, (%esp)             # ptr to address of string
    mov %esp, %esi              # save base of vector
    mov %esp, %edx              # also for iteration
2:  mov (%edx), %ebx            # get B string
    or %ebx, %ebx               # are we done?
    jz 1f                       # yes
    call scstr                  # fix string
    push %edi                   # save string fixup pointer
    add $4, %edx                # next string
    jmp 2b                      # keep going
1:
    mov $SYSEXECVE, %eax
    mov %esi, %ecx
    mov (%esi), %ebx
    mov envp, %edx
    int $0x80

    # NB we only get here if we failed
2:  cmp %esi, %esp
    jz 1f
    pop %edi
    movb $0xff, (%edi)
    jmp 2b
1:  leal -8(%ebp), %esp
    pop %ebx
    pop %ecx
    pop %ebp
    push %eax
    jmp *(%ecx)


################################################################################
#
# calls which aren't implemented on Linux
#

    mkncall gtty
    push $-1
    jmp *(%ecx)

    mkncall stty
    push $-1
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
    repne scasb         # edi -> just past it
    dec %edi            # -> nul
    movb $0, (%edi)
    pop %eax
    pop %ecx
    ret


