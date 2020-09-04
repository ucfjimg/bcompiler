#
# For now, build all the runtime and hello world 
#
all: a.out hello.lst printf.lst printn.lst blib.lst bsys.lst

a.out: b0.o hello.o printf.o printn.o libb.a
	ld -m elf_i386 b0.o hello.o printf.o printn.o libb.a

../programs/printf.b.o: ../programs/printf.b
	bc ../programs/printf.b

../programs/printn.b.o: ../programs/printn.b
	bc ../programs/printn.b

../programs/hello.b.o: ../programs/hello.b
	bc ../programs/hello.b

../programs/printf.b.s: ../programs/printf.b.o
	ba ../programs/printf.b.o

../programs/printn.b.s: ../programs/printn.b.o
	ba ../programs/printn.b.o

../programs/hello.b.s: ../programs/hello.b.o
	ba ../programs/hello.b.o

printf.lst printf.o: ../programs/printf.b.s
	as -a=printf.lst -o printf.o -a -march=i386 --32 ../programs/printf.b.s

printn.lst printn.o: ../programs/printn.b.s
	as -a=printn.lst -o printn.o -a -march=i386 --32 ../programs/printn.b.s

hello.lst hello.o: ../programs/hello.b.s
	as -a=hello.o -o hello.o -a -march=i386 --32 ../programs/hello.b.s

b0.o: b0.s
	as -o b0.o -a -march=i386 --32 b0.s

blib.lst blib.o: blib.s
	as -a=blib.lst -o blib.o -a -march=i386 --32 blib.s

bsys.lst bsys.o: bsys.s
	as -a=bsys.lst -o bsys.o -a -march=i386 --32 bsys.s

libb.a: blib.o bsys.o
	ar rcs -o libb.a blib.o bsys.o

clean:
	rm *.o *.a ../programs/*.b.o ../programs/*.b.s *.lst

