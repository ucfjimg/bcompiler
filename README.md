# A B Compiler

This project came out of my desire to implement a full compiler that went farther than a class project. I wanted to build something that compiled a useful language, and that produced native code. However, I still wanted something that was fairly simple.

I chose B as it has enough similarities to C to be familiar (it was C's predecessor) but scaled down, lacking such niceties as the preprocessor. The specification is the [users's manual for the PDP-11 version](https://www.bell-labs.com/usr/dmr/www/kbman.html) of the language.

In modern native languages, semantic constructs are translated directly into machine instructions that implement them. For example, a loop will have some instructions at the top to set the loop up, more instructions at the top and bottom to evaluated and test the loop condition, and a conditional branch to repeat the loop's body. The original B implementation, however, was a *threaded interpreter*. In this scheme, there are a number of small code blocks which implement what can be thought of as higher level opcodes which perform operations the compiler needs often. A program is then just a list of addresses of these code blocks, interespered with the operands to each pseudo-op. Each pesudo-op ends with a jump to a register which contains the address of the next pseudo-op. In a system like the PDP-11, which is very memory constrained but does not have much penalty for a branch, this allowed much smaller binaries for not much performance cost.

For a modern machine, better performance could be had by just translating to real instructions. I opted to implement the threaded interpreter model in the spirit of the original implementation. However, there is no PDP-11 code generation at this point.



