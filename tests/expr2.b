main()
{
    extrn printf;

    printf("%d*n", 1 < 2);
    printf("%d*n", 2 < 2);
    printf("%d*n", 3 < 2);

    printf("%d*n", 1 <= 2);
    printf("%d*n", 2 <= 2);
    printf("%d*n", 3 <= 2);

    printf("%d*n", 1 > 2);
    printf("%d*n", 2 > 2);
    printf("%d*n", 3 > 2);

    printf("%d*n", 1 >= 2);
    printf("%d*n", 2 >= 2);
    printf("%d*n", 3 >= 2);

    printf("%d*n", 3 == 2);
    printf("%d*n", 2 == 2);

    printf("%d*n", 3 != 2);
    printf("%d*n", 2 != 2);
}
