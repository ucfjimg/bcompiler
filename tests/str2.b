main()
{
    extrn printf, lchar;
    auto i, str;

    str = "        ";

    i = -1;
    while (++i < 8)
        lchar(str, i, '!' + i);

    printf("%s*n", str);
}
