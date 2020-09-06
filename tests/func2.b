main()
{
    extrn hello, printf;
    printf("%s*n", hello());
}

hello()
{
    return ("hello world");
}
