main()
{
    extrn execl, printf;
    auto pgm, arg;

    pgm = "/biiin/ls";
    arg = "-la";

    if (execl(pgm, arg, 0) < 0) {
        printf("exec failed*n");
    }

    printf("%s*n%s*n", pgm, arg);

    pgm = "/bin/ls";
    arg = "-la";

    if (execl(pgm, arg, 0) < 0) {
        printf("exec failed*n");
    }

    printf("%s*n%s*n", pgm, arg);
}
