main()
{
    extrn add, printf;

    printf("%d*n", add(123, 321));
}

add(a, b)
{
    return(a+b);
}
