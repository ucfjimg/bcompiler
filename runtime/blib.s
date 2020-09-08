#
# B threaded opcodes
#

################################################################################
#
# Register usage
#
# ECX - Instruction pointer into current routine. At the start of an instruction,
#       ecx points to a function pointer for the threaded opcode to execute.
#       Instruction encoded operands follow the threaded opcode address and are
#       accessed relative to ecx. Every threaded opcode should end by incrementing
#       ecx past the instruction and executing jmp *(%ecx), which will jump to
#       the start of the next opcode.
#
# EBX - The temporary register. The opcodes PUSHT and POPT transfer the top of the
#       stack into EBX and back, to aid in stack juggling. Other opcode 
#       implementations should therefore not use EBX.
#
# EBP - Frame pointer. EBP points to the activation record for the current function
#       call. Arguments to the function are at EBP+8, EBP+12, etc. The leftmost
#       argument is the last argument pushed. EBP+4 contains the return address
#       (the value for ECX of the next instruction to execute upon return.) 
#       EBP+0 contains the frame pointer of the previous function invocation.
#       Negative offsets contain space allocated for auto variables.
#
# Pointers
#
#   B is a typeless language. All objects -- integers and pointers -- are of the
#   same size (4 bytes for x86). Vectors are a pointer to an array of words.
#   Strings are represented as an array of words, where each element contains 
#   4 packed characters.
#
#   The add and subtract operators are expected to work on pointers. Adding one
#   to a pointer gives a pointer to the next word. Subtracting two pointers gives
#   the number of words between them. As B is typeless, there is no hint at 
#   compilation that an operand is an address or an integer. (The users is also
#   free to do such nonsenical things as adding two pointers, with no error
#   given.) In order for this to work, a pointer cannot be equivalent to an 
#   address. Instead, the pointer is the address, shifted right by the number of
#   bits needed to divide by the native word size (2 bits for 4 bytes on x86.)
#   All addresses must therefore be aligned on 4 byte boundaries.
#
#   Pointers in extrn data, and function pointers, are shifted during dopinit
#   before main() is called. Pointers loaded during the course of program 
#   execution are shifted right when loaded, and again to the left before use.
#
# This architecture is very similar to that described in the original B language 
# memo for the PDP-11 implementation.
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


#
# Initialize the base address of a vector on the stack
#
    .global AVINIT
AVINIT:
    mov 4(%ecx), %eax   # stack offset of base of vector
    add %ebp, %eax      # eax -> vector pointer
    leal 4(%eax), %edx  # edx -> base of vector
    shr $2, %edx        # edx is adjusted pointer
    mov %edx, (%eax)    # save adjusted vector
    add $8, %ecx
    jmp *(%ecx)

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
    jnz 1f              # nope
    mov %eax, %ecx      # yes, jump to target
1:
    jmp *(%ecx)
    
#
# case statement
#
    .global CASE
CASE:
    mov 4(%ecx), %eax   # case value
    mov 8(%ecx), %edx   # start of routine
    cmp %eax, (%esp)    # disc is on top of stack
    je 1f               # a match!
    add $12, %ecx       # skip args
    jmp *(%ecx)         # and keep going
1:
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
    push 4(%ecx)
    add $8, %ecx        
    jmp *(%ecx)

#
# Push the EXTRN onto the stack. This is a pointer so it must be
# aligned and will be shifted remove the always zero bits.
#
    .global PSHSYM
PSHSYM:
    movl 4(%ecx), %eax
    shr $2, %eax        # address to object index
    push %eax
    add $8, %ecx       
    jmp *(%ecx)

#
# Push the lvalue of the given auto variable or argument 
# onto the stack
#
    .global PSHAUTO
PSHAUTO:
    movl 4(%ecx), %eax
    add %ebp, %eax
    shr $2, %eax
    push %eax
    add $8, %ecx        
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
    mov 4(%ecx), %esi
    mov (%esp,%esi), %eax
    push %eax
    add $8, %ecx       
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
    mov 4(%ecx), %eax   # eax is aligned bytes to pop
    add %eax, %esp      # do the pop
    add $8, %ecx        # past the argument
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
    push 4(%ecx)        # entry to function
    add $8, %ecx        # past argument
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


