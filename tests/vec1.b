main()
{
    extrn n, v, printf;
    auto i, j;

    i = 0;
    j = 0;
    while (i < n)  
        v[i++] = 1 << j++;

    i = 0;
    while (i < n)
        printf("%d*n", v[i++]);
}

n 10;
v[10];
