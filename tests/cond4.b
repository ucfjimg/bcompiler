main()
{
    extrn printf;
    auto i;

    i = 0;
    while (i < 3) {
        switch i {
            case 0:
                printf("zero*n");

            case 1:
                printf("one*n");

            case 2:
                printf("two*n");
        }
        i++;
    }
}
