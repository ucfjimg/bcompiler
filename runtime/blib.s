#
# B threaded opcodes
#
    .text

#
# Set the stack frame at the start of a function. The return address is 
# already on the top of the stack. Pushes the old frame pointer, sets up
# the new frame pointer, and allocates space for auto's on the stack (# of
# bytes as the argument). # of bytes to allocate must be aligned.
#
    .global ENTER
ENTER:
    push %ebp           # set up
    mov %esp, %ebp      # stack frame
    subl (%ecx), %esp   # allocate space
    add $4, %ecx        
    jmp *(%ecx)

#
# Leave a function: restore the stack and frame pointer to their values
# at original invocation.
#
    .global LEAVE
LEAVE:
    mov %ebp, %esp      # clean up autos from stack
    pop %ebp            # restore previous frame ptr
    add $4, %ecx        
    jmp *(%ecx)

#
# Push the constant in the arguments on the stack.
#
    .global PSHCON
PSHCON:
    add $4, %ecx        # -> arg (stack space)
    push (%ecx)
    add $4, %ecx        
    jmp *(%ecx)

#
# Push the EXTRN onto the stack. This is a pointer so it must be
# aligned and will be shifted remove the always zero bits.
#
    .global PSHSYM
PSHSYM:
    add $4, %ecx        # -> arg (stack space)
    movl (%ecx), %eax
    shr $2, %eax        # address to object index
    push %eax
    add $4, %ecx       
    jmp *(%ecx)

#
# dereference the (shifted) address on the stack
#
    .global DEREF
DEREF:
    pop %edx            # shiftd pointer
    shl $2, %edx        # raw pointer
    push (%edx)         # push the pointed-to word
    add $4, %ecx       
    jmp *(%ecx)


#
# Pushes the temporary register onto the stack.
#

    .global PUSHT
PUSHT:
    push %ebx
    add $4, %ecx       
    jmp *(%ecx)

#
# Pops the stack into the temporary register.
#
    .global POPT
POPT:
    pop %ebx
    add $4, %ecx       
    jmp *(%ecx)

#
# Duplicates the stack object at an offset given by the
# argument, which must be aligned.
#
    .global DUPN
DUPN:
    add $4, %ecx       
    mov (%ecx), %esi
    mov (%ebp,%esi), %eax
    push %eax
    add $4, %ecx       
    jmp *(%ecx)
    
#
# Call a function. The address is on the stack. Push the
# return address on the stack and start executing the function.
#
    .global CALL
CALL:
    add $4, %ecx        # past the instruction
    pop %eax            # %eax is the shifted entry address
    shr $2, %eax        # %eax to address
    push %ecx           # return address
    mov %eax, %ecx      # %ecx is function entry
    jmp *(%ecx)

#
# Return from a function. The stack should already have been
# cleaned up. Before the return address is the return value,
# which needs to be preserved.
#
# TODO it would save some stack manipulation to defined the
# return value as being in a CPU register.
#
    .global RET
RET:
    pop %eax            # return value
    pop %ecx            # return address
    push %eax           # return value
    jmp *(%ecx)         # continue back in caller

#
# pop one thing off the stack
#
    .global POP
POP:
    pop %eax            # pop and discard
    add $4, %ecx       
    jmp *(%ecx)

#
# pop 'n' things off the stack
#
    .global POPN
POPN:
    add $4, %ecx        # past the instruction
    mov (%ecx), %eax    # eax is aligned bytes to pop
    add %eax, %esp      # do the pop
    add $4, %ecx        # past the argument
    jmp *(%ecx)

#
# TODO this is a compiler placeholder and shouldn't be
# emitted
#
    .global NAMDEF
NAMDEF:
    add $4, %ecx        # past the instruction
    jmp *(%ecx)

#
# native call. this only occurs in library functions,
# and the pointer is not shifted. the native code
# is expected to remember ecx and eventually use it
# to restart threaded executiion.
#
    .global NCALL
NCALL:
    add $4, %ecx        # past the instruction
    push (%ecx)         # entry to function
    add $4, %ecx        # past argument
    ret
    
################################################################################
#
# math
#
    .global ADD
ADD:
    pop %eax            # rhs
    pop %edx            # lhs
    add %edx, %eax
    push %eax
    add $4, %ecx        # past argument
    ret

