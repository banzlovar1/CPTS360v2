#include "commands.h"

int cp_file(char *src, char *dest)
{
    int n = 0;
    char buf[BLKSIZE];
    int fd = open_file(src, "0");
    int gd = open_file(dest, "1");
    if(gd < 0)
    {
        printf("Creating File\n");
        int ino = creat_file(dest);
        gd = open_file(dest, "1");;
    }
    /* Need read to complete
    while( n = myread(fd, buf, BLKSIZE))
    {
        mywrite(gd, buf, n);
    }
    */
    close_file(fd);
    close_file(gd);
    
    return 0;

}