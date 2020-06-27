#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>

#include "server.h"


char *print_addr(struct sockaddr_in *addr)
{
    uint32_t ip = ntohl(addr->sin_addr.s_addr);
    uint16_t port = ntohs(addr->sin_port);
    // maks: xxx.xxx.xxx.xxx:xxxxx
    char *buff = malloc(22);
    sprintf(buff, "%d.%d.%d.%d:%d",
            ip >> 24 & 0xff,
            ip >> 16 & 0xff,
            ip >> 8 & 0xff,
            ip >> 0 & 0xff,
            port);
    return buff;
}

int server_start(){
    uint32_t listenAddr = INADDR_ANY;
    uint16_t port = 1234U;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serveraddr;

    if (fd == -1)
    {
        fprintf(stderr, "Fail creating socket\n");
        return 1;
    }
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(listenAddr);
    serveraddr.sin_port = htons(port);
    printf("Listening on %s ", print_addr(&serveraddr));

    if (bind(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == 0)
    {
        if (listen(fd, 5) == 0)
        {
            printf("OK\n");
            return 0;
        }
        else
        {
            printf("Failed to listen\n");
            close(fd);
            return 1;
        }
    }
    else
    {
        printf("Fail bind\n");
        close(fd);
        return 1;
    }

}