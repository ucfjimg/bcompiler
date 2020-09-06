inc(ptri)
{
    (*ptri)++;
}

main()
{
    extrn printf;

    auto i;

    i = 41;
    inc(&i);
    printf("%d*n", i);
}
