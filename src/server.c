#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>

#include "server.h"
#include "screen.h"

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

void handle_client(int *fd, struct sockaddr_in *client)
{
}

int server_start()
{
    uint16_t port = 1234U;
    struct sockaddr_in serveraddr, client;
    socklen_t addrLen = sizeof(struct sockaddr_in), clientLen;
    int client_fd, fd = socket(AF_INET, SOCK_STREAM, 0), opt_reuseaddr = 1;
    int bufSize = 65535, recvd;
    char *buf = malloc(bufSize);
    if (fd == -1)
    {
        fprintf(stderr, "Fail creating socket\n");
        return 1;
    }
    memset(&serveraddr, 0, addrLen);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);
    printf("Listening on %s ", print_addr(&serveraddr));
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt_reuseaddr, sizeof(opt_reuseaddr));

    if (bind(fd, (struct sockaddr *)&serveraddr, addrLen) == 0)
    {
        if (listen(fd, 5) == 0)
        {
            printf("OK\n");
            while (1)
            {
                clientLen = addrLen;
                memset(&client, 0, addrLen);
                client_fd = accept(fd, (struct sockaddr *)&client, &clientLen);
                if (client_fd >= 0)
                {
                    recvd = recv(client_fd, buf, bufSize, 0);
                    if (recvd > 0)
                    {
                        buf[recvd] = 0;
                        printf("%s", buf);
                        recvd = sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type:image/png\r\n\r\n");
                        send(client_fd, buf, recvd, 0);
                        screen_grab(client_fd);
                        //fflush(&client_fd);
                        close(client_fd);
                    }
                    else
                    {
                        printf("Ignore recv %d bytes\n", recvd);
                    }
                }
                else
                {
                    printf("Fail accepting client\n");
                }
            }
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