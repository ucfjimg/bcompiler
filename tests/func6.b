hello()
{
    extrn printf;
    printf("hello, world!");
}

main()
{
    extrn hello, putchar;
    auto fnp;

    fnp = hello;
    fnp();
    putchar('*n');
}
