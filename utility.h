#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//print error
void error(char *msg)
{
    perror(msg);
    exit(0);
}

