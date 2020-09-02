c /**/ [] 1, 2, 3;
a;
b 1;
d;
bar 'ab';
foo[4] "abc*ndef", 42, a, b;
smag a,b;

main()
{
    auto e;
    extrn putchar;


    -c[42]--;
    ++b;
    b * c[0]; 
    1+2*3 << (a+3);
    a+3 < 47;
    a == 0;

    return (1+2);

    goto here;

    a & 42 | b;
    
    e = a + 12;

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
        case 1: putchar('1');
        case 2: putchar('2');
    }

    b =+ 2;
    b =- 2;
}

f2() {
}


