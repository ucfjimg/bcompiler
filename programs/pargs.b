line[20];

/**
 left: 
  ctime
  execl
  execv
  fstat
  stat
  time
 */

main()
{
    extrn argv, printf, open, close, read, lchar, creat, write, chdir, exit;
    extrn chmod, chown, fork, wait, getuid, link, mkdir, seek, setuid, unlink;
    extrn gtty, stty, putchar;
    auto i, j, fd;

    if (argv[0] > 1) {
        printf("=== reading from %s*n", argv[2]);
        fd = open(argv[2], 0);
        if (fd >= 0) {
            printf("head of file*n");
            i = read(fd, line, 79);
            lchar(line, i, '*e');
            printf("%s*n", line);
            seek(fd, -79, 2);
            i = read(fd, line, 79);
            lchar(line, i, '*e');
            printf("tail of file*n");
            printf("%s*n", line);
            close(fd);
        }
    }

    if (argv[0] > 2) {
        fd = argv[3];
        printf("=== unlink(%d)*n", fd);
        if (unlink(fd) < 0) {
            printf("failed to unlink*n");
        }
    }

    printf("=== chdir(/tmp)*n");
    if (chdir("/tmp") < 0) {
        printf("failed to change directory*n");
        exit(1);
    }

    printf("=== creat(foo.txt, 0666)*n");
    fd = creat("foo.txt", 0666);
    if (fd < 0) {
        printf("creat failed %d*n", fd); 
    } else {
        write(fd, "hello, world!*n", 14);
        close(fd);
    }
    printf("=== chmod(foo.txt, 0600)*n");
    chmod("foo.txt", 0600);
    
    printf("=== chown(foo.txt, 1001)");
    if (getuid()) {
        printf(" (will fail, not root)");
    } 
    putchar('*n');
    if ((i = chown("foo.txt", 1001)) < 0) 
        printf("chown() failed %d*n", i);

    printf("=== fork()*n");
    i = fork();
    if (i < 0) {
        printf("fork() failed %d*n", i);
    } else if (i > 0) {
        j = wait();
        printf("child %d started; child %d finished.*n", i, j);
    } else {
        i = 0;
        while (i < 10) 
            printf("child iteration %d*n", i++);
        exit(0);
    }
    
    printf("my uid is %d*n", i = getuid());
    if (setuid(1001) < 0) {
        printf("cannot change uid (are you root?)*n");
    } else {
        printf("uid now %d*n", getuid());
    }

    printf("=== link(/tmp/foo.txt, /tmp/bar.txt)*n");
    if ((i = link("/tmp/foo.txt", "/tmp/bar.txt")) < 0) {
        printf("link: %d*n", i);
    }

    printf("=== mkdir(/tmp/jimge)*n");
    if (mkdir("/tmp/jimge", 0755) < 0) 
        printf("mkdir failed*n");

    if (gtty() < 0) printf("gtty failed (expected)*n");
    if (stty() < 0) printf("stty failed (expected)*n");
}

