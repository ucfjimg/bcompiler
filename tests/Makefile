all: \
	output1 output2 output3 output4 output5 \
	cond1 cond2 cond3 cond4 cond5 \
	func1 func2 func3 func4 func5 \
	expr1 expr2 expr3 expr4 expr5 \
	vec1

%: %.b 
	b -pgl -o $* $< ../programs/printf.b ../programs/printn.b
	./$* | diff - $*.out

output1: output1.b 
output2: output2.b
output3: output3.b
output4: output4.b
output5: output5.b

cond1: cond1.b
cond2: cond2.b
cond3: cond3.b
cond4: cond4.b
cond5: cond5.b

func1: func1.b
func2: func2.b
func3: func3.b
func4: func4.b
func5: func5.b

expr1: expr1.b
expr2: expr2.b
expr3: expr3.b
expr4: expr4.b
expr5: expr5.b

vec1: vec1.b

clean:
	-rm *.i > /dev/null 2>&1
	-rm *.o > /dev/null 2>&1
	-rm *.map > /dev/null 2>&1
	-rm *.lst > /dev/null 2>&1
	-rm *.s > /dev/null 2>&1
	-rm `find -executable -a -type f` > /dev/null 2>&1
