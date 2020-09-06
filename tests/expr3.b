main()
{
    extrn printf;

    printf("%d*n", 6|3); /* 110 | 011 => 111 (7) */
    printf("%d*n", 6&3); /* 110 & 011 => 010 (2) */
}
