#
# For now, build all the runtime and hello world 
#
all: a.out 

a.out: b0.o hello.o printf.o printn.o libb.a
	ld -m elf_i386 b0.o hello.o printf.o printn.o libb.a

%.i: ../programs/%.b
	bc -o $*.i $< > $*.icode

%.s: %.i
	ba -o $*.s $<

%.o: %.s
	as -g -a=$*.lst -o $*.o -a -march=i386 --32 $< 

printf.i: ../programs/printf.b
printn.i: ../programs/printn.b
hello.i: ../programs/hello.b

printf.s: printf.i
printn.s: printn.i
hello.s: hello.i

printf.o: printf.s
printn.o: printn.s
hello.o: hello.s
b0.o: b0.s
blib.o: blib.s
bsys.o: bsys.s
bstr.o: bstr.s

libb.a: blib.o bsys.o bstr.o
	ar rcs -o libb.a blib.o bsys.o bstr.o

clean:
	rm *.i *.o *.a *.lst

