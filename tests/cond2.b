main()
{
    extrn printf;
    auto i,j;

    i = 1;
    j = 0;

    /* else goes with innermost if */
    if (i)
        if (j)
            printf("yes*n");
        else
            printf("no*n");
}
