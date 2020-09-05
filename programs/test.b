main()
{
    extrn printf;


    printf("%d*n", 3 * 4);
    printf("%d*n", !3);
    printf("%d*n", !0);
    
    printf("%d*n", 1 << 3);
    printf("%d*n", 1024 >> 2); 

    printf("%d*n", 5 | 12);
    printf("%d*n", 15 & (16 + 8)); 
}
