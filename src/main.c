#include <stdio.h>
#include "server.h"


int main(int argc, char const *argv[])
{
    printf("YES, IM main...\n");
    return server_start();
}
