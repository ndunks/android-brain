#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>
#include <binder/ProcessState.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include <ui/PixelFormat.h>
#include <SkImageEncoder.h>
#include <SkBitmap.h>
#include <SkData.h>
#include <SkStream.h>

#include "server.h"

using namespace android;

static SkBitmap::Config flinger2skia(PixelFormat f)
{
    switch (f)
    {
    case PIXEL_FORMAT_RGB_565:
        return SkBitmap::kRGB_565_Config;
    default:
        return SkBitmap::kARGB_8888_Config;
    }
}
static const char http_version[] = "HTTP/1.1",
                  header_ok[] = "200 OK",
                  header_bad_request[] = "400 Bad Request",
                  header_not_found[] = "404 Not Found",
                  header_not_allowed[] = "405 Method Not Allowed",
                  header_internal_error[] = "500 Internal Server Error",
                  header_content[] = "Connection: close\r\nContent-Length: %d\r\nContent-Type: %s";

static size_t http_header(char *buf, const char *http_status, const char *type, size_t size)
{
    if (type == NULL)
    {
        type = "text/plain";
    }

    size_t header_size = sprintf(buf, "%s %s\r\n", http_version, http_status);
    header_size += sprintf(buf + header_size, header_content, size, type);
    strcpy(buf + header_size, "\r\n\r\n");
    header_size += 4;
    return header_size;
}

static void handle_connection(int fd)
{
    int client_fd, bufSize = 65535;
    uint32_t w, s, h, f;
    struct sockaddr_in client;
    socklen_t clientLen;
    char *buf = (char *)malloc(bufSize), *path;
    ProcessState::self()->startThreadPool();
    void const *base = 0;
    SkDynamicMemoryWStream stream;
    size_t size = 0;
    ScreenshotClient screenshot;
    SkBitmap b;
    SkData *streamData;

    sp<IBinder> display = SurfaceComposerClient::getBuiltInDisplay(ISurfaceComposer::eDisplayIdMain);
    if (display == NULL)
    {
        fprintf(stderr, "Can't open display\n");
        return;
    }

    while (1)
    {
        clientLen = sizeof(client);
        memset(&client, 0, clientLen);
        printf("Waiting connection...\n");
        client_fd = accept(fd, (struct sockaddr *)&client, &clientLen);
        if (client_fd < 0)
        {
            printf("Fail accepting client\n");
            continue;
        }
        size = recv(client_fd, buf, bufSize, 0);
        if (size < 0)
        {
            printf("Ignore recv %d bytes\n", size);
            continue;
        }
        buf[size] = 0;
        printf("%s", buf);

        if (strncmp(buf, "GET", 3) != 0)
        {
            size = http_header(buf, header_not_allowed, NULL, 0);
            write(client_fd, buf, size);
        }
        if (strncmp(buf + 3, " / ", 3) != 0)
        {
            size = http_header(buf, header_not_found, NULL, 0);
            write(client_fd, buf, size);
        }
        else
        {
            if (screenshot.update(display, 360, 640) == NO_ERROR)
            {
                base = screenshot.getPixels();
                w = screenshot.getWidth();
                h = screenshot.getHeight();
                s = screenshot.getStride();
                f = screenshot.getFormat();
                b.setConfig(flinger2skia(f), w, h, s * bytesPerPixel(f));
                b.setPixels((void *)base);
                SkImageEncoder::EncodeStream(&stream, b,
                                             SkImageEncoder::kWEBP_Type, SkImageEncoder::kDefaultQuality);
                streamData = stream.copyToData();
                size = http_header(buf, header_ok, "image/webp", streamData->size());
                write(client_fd, buf, size);
                write(client_fd, streamData->data(), streamData->size());
                streamData->unref();
                stream.reset();
            }
            else
            {
                fprintf(stderr, "Error capturing screen\n");
                size = http_header(buf, header_internal_error, "text/plain", 0);
                write(client_fd, buf, size);
            }
        }
        close(client_fd);
    }
}

int server_start()
{
    uint16_t port = 1234U;
    struct sockaddr_in serveraddr;
    socklen_t addrLen = sizeof(struct sockaddr_in);
    int fd = socket(AF_INET, SOCK_STREAM, 0), opt_reuseaddr = 1;

    if (fd == -1)
    {
        fprintf(stderr, "Fail creating socket\n");
        return 1;
    }
    memset(&serveraddr, 0, addrLen);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);
    printf("Listening on port %d\n", port);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt_reuseaddr, sizeof(opt_reuseaddr));

    if (bind(fd, (struct sockaddr *)&serveraddr, addrLen) == 0)
    {
        if (listen(fd, 5) == 0)
        {
            printf("OK\n");
            handle_connection(fd);
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
    printf("Exited\n");
}