main()
{
    extrn execl, printf;

    if (execl("/bin/ls", "-la", 0) < 0) {
        printf("exec failed*n");
    }
}
