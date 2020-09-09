line[20];


main()
{
    extrn argv, printf, open, close, read, lchar, creat, write, chdir, exit;
    extrn chmod, chown, fork, wait;
    auto i, j, fd;

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

    if (chdir("/tmp") < 0) {
        printf("failed to change directory*n");
        exit(1);
    }
    fd = creat("foo.txt", 0666);
    write(fd, "hello, world!*n", 14);
    close(fd);

    chmod("foo.txt", 0600);
    
    if ((i = chown("foo.txt", 1001)) < 0) 
        printf("chmod() failed %d*n", i);

    i = fork();
    if (i < 0) {
        printf("fork() failed %d*n", i);
    } else if (i > 0) {
        j = wait();
        printf("child %d started; child %d finished.*n", i, j);
    } else {
        /* child */
        i = 0;
        while (i < 100) 
            printf("child iteration %d*n", i++);
    }
}
