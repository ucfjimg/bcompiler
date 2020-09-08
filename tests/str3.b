main()
{
    extrn printf, lchar;
    auto i, str 3;

    i = -1;
    while (++i < 8)
        lchar(str, i, '!' + i);
    lchar(str, 8, '*e');

    printf("%s*n", str);
}
