line[20];


main()
{
    extrn argv, printf, open, close, read, lchar, creat, write;
    auto i, fd;

    if (argv[0] > 1) {
        fd = open(argv[2], 0);
        if (fd >= 0) {
            printf("%s:*n*n", argv[2]);
            i = read(fd, line, 79);
            lchar(line, i, '*e');
            printf("%s*n", line);
            close(fd);
        }
    }

    fd = creat("foo.txt", 0666);
    write(fd, "hello, world!*n", 14);
    close(fd);
}
