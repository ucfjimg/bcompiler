#
# B threaded opcodes
#
    .text


################################################################################
#
# function calls
#

#
# Call a function. The address is on the stack. Push the
# return address on the stack and start executing the function.
#
    .global CALL
CALL:
    add $4, %ecx        # past the instruction
    pop %eax            # %eax is the shifted entry address
    shl $2, %eax        # %eax to address
    push %ecx           # return address
    mov %eax, %ecx      # %ecx is function entry
    jmp *(%ecx)

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
    add $4, %ecx        
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

################################################################################
#
# control transfer
#

#
# jump to the argument
#
    .global JMP
JMP:
    add $4, %ecx        # -> arg (stack space)
    mov (%ecx), %ecx    # jump
    jmp *(%ecx)
    
#
# branch if the top of the stack is zero 
#
    .global BZ
BZ:
    add $4, %ecx        # -> arg (stack space)
    mov (%ecx), %eax    # jump target
    add $4, %ecx        
    pop %edx            # condition
    or %edx, %edx       # is condition zero?
    jnz bzno            # nope
    mov %eax, %ecx      # yes, jump to target
bzno:
    jmp *(%ecx)
    
#
# case statement
#
    .global CASE
CASE:
    mov 4(%ecx), %eax   # case value
    mov 8(%ecx), %edx   # start of routine
    cmp %eax, (%esp)    # disc is on top of stack
    je casego           # a match!
    add $12, %ecx       # skip args
    jmp *(%ecx)         # and keep going
casego:
    pop %eax            # pop disc off stack
    mov %edx, %ecx      # new instruction ptr
    jmp *(%ecx)

################################################################################
#
# stack manipulation 
#

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
# Push the lvalue of the given auto variable or argument 
# onto the stack
#
    .global PSHAUTO
PSHAUTO:
    add $4, %ecx        # -> arg (stack space)
    movl (%ecx), %eax
    add %ebp, %eax
    shr $2, %eax
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
# store a value into memory. top of stack is value,
# second on stack is address. 
#
    .global STORE
STORE:
    pop %eax            # value to store
    pop %edx            # pointer
    shl $2, %edx
    movl %eax, (%edx)
    add $4, %ecx       
    jmp *(%ecx)

#
# rotate the top three elements on the stack such
# that
# a2 a1 a0 [ROT] a0 a2 a1 
#
    .global ROT
ROT:
    pop %eax            # a0
    pop %edx            # a1
    pop %esi            # a2

    push %eax           # a0
    push %esi           # a2
    push %edx           # a1

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
# Duplicates the top object on the stack
#
    .global DUP
DUP:
    push (%esp)
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
    mov (%esp,%esi), %eax
    push %eax
    add $4, %ecx       
    jmp *(%ecx)

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
# to restart threaded execution.
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

#
# a1 a0 [ADD] a1+a0
#
    .global ADD
ADD:
    pop %eax            # rhs
    pop %edx            # lhs
    add %edx, %eax
    push %eax
    add $4, %ecx        # past argument
    jmp *(%ecx) 

#
# a1 a0 [SUB] a1-a0
#
    .global SUB 
SUB:
    pop %eax            # rhs
    pop %edx            # lhs
    sub %eax, %edx
    push %edx
    add $4, %ecx        # past argument
    jmp *(%ecx) 

#
# a0 [NEG] -a0
#
    .global NEG
NEG:
    negl (%esp)
    add $4, %ecx        # past argument
    jmp *(%ecx) 

#
# a0 [NOT] !a0
#
    .global NOT
NOT:
    pop %eax
    push $0
    cmp $0, %eax
    setz (%esp)
    add $4, %ecx        # past argument
    jmp *(%ecx) 

#
# a1 a0 [SHR] a1>>a0 
#
    .global SHR
SHR:
    pop %edi            # rhs
    pop %eax            # lhs
    xchg %ecx, %edi     # shift must be in cl
    shr %cl, %eax
    push %eax
    leal 4(%edi), %ecx
    jmp *(%ecx) 

#
# a1 a0 [SHL] a1<<a0 
#
    .global SHL
SHL:
    pop %edi            # rhs
    pop %eax            # lhs
    xchg %ecx, %edi     # shift must be in cl
    shl %cl, %eax
    push %eax
    leal 4(%edi), %ecx
    jmp *(%ecx) 

#
# a1 a0 [AND] a1&a1
#
    .global AND
AND:
    pop %edi            # rhs
    and %edi, (%esp)
    add $4, %ecx        # past argument
    jmp *(%ecx) 

#
# a1 a0 [OR] a1|a1
#
    .global OR
OR:
    pop %edi            # rhs
    or %edi, (%esp)
    add $4, %ecx        # past argument
    jmp *(%ecx) 



#
# a1 a0 [DIV] a1/a0
#
# IDIV xxx: EDX:EAX / xxx => EAX (rem EDX)
#
    .global DIV
DIV:
    pop %edi            # rhs
    pop %eax            # lhs
    mov %eax, %edx
    sar $31, %edx       # sign ex eax into edx
    idiv %edi, %eax
    push %eax
    add $4, %ecx        # past argument
    jmp *(%ecx) 

#
# a1 a0 [MOD] a1/a0
#
# IDIV xxx: EDX:EAX / xxx => EAX (rem EDX)
#
    .global MOD
MOD:
    pop %edi            # rhs
    pop %eax            # lhs
    mov %eax, %edx
    sar $31, %edx       # sign ex eax into edx
    idiv %edi, %eax
    push %edx
    add $4, %ecx        # past argument
    jmp *(%ecx) 

#
# a1 a0 [MUL] a1*a0
#
    .global MUL
MUL:
    pop %edi            # rhs
    pop %eax            # lhs
    imul %edi
    push %eax
    add $4, %ecx
    jmp *(%ecx)

#
# a1 a0 [EQ] a1==a0 ? 1 : 0
#
    .global EQ
EQ:
    pop %eax
    pop %edx
    push $0
    cmp %edx, %eax
    setz (%esp)
    add $4, %ecx        # past argument
    jmp *(%ecx) 

#
# a1 a0 [NE] a1!=a0 ? 1 : 0
#
    .global NE
NE:
    pop %eax
    pop %edx
    push $0
    cmp %edx, %eax
    setnz (%esp)
    add $4, %ecx        # past argument
    jmp *(%ecx) 

#
# a1 a0 [LT] a1<a0 ? 1 : 0
#
    .global LT 
LT:
    pop %eax
    pop %edx
    push $0
    cmp %eax, %edx
    setl (%esp)
    add $4, %ecx        # past argument
    jmp *(%ecx) 

#
# a1 a0 [LE] a1<a0 ? 1 : 0
#
    .global LE 
LE:
    pop %eax
    pop %edx
    push $0
    cmp %eax, %edx
    setle (%esp)
    add $4, %ecx        # past argument
    jmp *(%ecx) 

#
# a1 a0 [GT] a1<a0 ? 1 : 0
#
    .global GT 
GT:
    pop %eax
    pop %edx
    push $0
    cmp %eax, %edx
    setg (%esp)
    add $4, %ecx        # past argument
    jmp *(%ecx) 

#
# a1 a0 [GE] a1<a0 ? 1 : 0
#
    .global GE 
GE:
    pop %eax
    pop %edx
    push $0
    cmp %eax, %edx
    setge (%esp)
    add $4, %ecx        # past argument
    jmp *(%ecx) 


