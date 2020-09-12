main()
{
    extrn execv, printf;
    auto pgm, arg 2;

    pgm = "/biiin/ls";
    arg[0] = "-la";

    if (execv(pgm, arg, 1) < 0) {
        printf("exec failed*n");
    }

    printf("%s*n%s*n", pgm, arg[0]);

    pgm = "/bin/ls";

    if (execv(pgm, arg, 1) < 0) {
        printf("exec failed*n");
    }

    printf("%s*n%s*n", pgm, arg[0]);
}
