c /**/ [] 1, 2, 3;
a;
b 1;
d;
bar 'ab';
foo[4] "abc*ndef", 42, a, b;
smag a,b;

main()
{
    extrn putchar;

    -c[42]--;
    ++b;
    b * c[0]; 
    1+2*3 << (a+3);
    a+3 < 47;
    a == 0;

    a & 42 | b;
here:
    a > 4 ? 4 : a;
    a = b = 1;
    c = *(&d + 1);
    
    if (c) {
        1;
    } else {
        2;
    }
    
    while (c--)
        a = a + 2;
    a(b+1,c+2) + b();
    putchar(a);

    switch a {

    }

}


