fact(n)
{
    if (n <= 1)
        return (1);
    return (n*fact(n-1));
}

main()
{
    extrn printf;

    printf("%d*n", fact(5));
}
