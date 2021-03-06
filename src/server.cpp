#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>
#include <binder/ProcessState.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include <binder/IServiceManager.h>
#include <powermanager/IPowerManager.h>
#include <ui/DisplayInfo.h>
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

static char buf[65535];
static ScreenshotClient screenshot;
uint32_t scalled_width, scalled_height;
static ssize_t size = 0, bpp;
static int sleeping_state;
static pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

sp<IBinder> display;
sp<IServiceManager> servicemanager;

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

void *sleeping_check_thread(void *arg)
{
    int fd_sleep, fd_wake;
    size_t ret;
    char ch;
    fd_sleep = open("/sys/power/wait_for_fb_sleep", O_RDONLY);
    fd_wake = open("/sys/power/wait_for_fb_wake", O_RDONLY);

    while (1)
    {
        ret = read(fd_sleep, &ch, 0);
        pthread_mutex_lock(&mutex1);
        sleeping_state = 1;
        pthread_mutex_unlock(&mutex1);
        printf("\n** Device Is sleep.\n");

        ret = read(fd_wake, &ch, 0);
        pthread_mutex_lock(&mutex1);
        sleeping_state = 0;
        pthread_mutex_unlock(&mutex1);
        printf("\n** Device Is wake.\n");
    }
    close(fd_sleep);
    close(fd_wake);
    printf("** Sleeping thread exited\n");
}

void send_text(int fd, const char *txt)
{
    size = http_header(buf, header_ok, "text/plain", strlen(txt));
    write(fd, buf, size);
    write(fd, txt, strlen(txt));
}
void send_dump(int fd)
{
    char buf[255];
    Vector<String16> args;
    sp<IBinder> service = servicemanager->checkService(String16("power"));

    ssize_t size = sprintf(buf, "%s %s\r\n"
                                "Connection: close\r\n"
                                "Content-Type: text/plain\r\n\r\n",
                           http_version, header_ok);
    write(fd, buf, size);

    printf("DUMP %s\n", buf);
    int err = service->dump(fd, args);
    if (err != 0)
        printf("error: get power service");
}
void send_screenshot(int fd)
{
    SkDynamicMemoryWStream stream;
    SkBitmap b;
    SkData *streamData;
    ssize_t bpp;
    SkBitmap::Config bmpConfig;
    uint32_t format, stride;
    int is_sleeping;
    pthread_mutex_lock(&mutex1);
    is_sleeping = sleeping_state;
    pthread_mutex_unlock(&mutex1);

    if (is_sleeping)
    {
        send_text(fd, "Device sleeping");
        return;
    }

    if (screenshot.update(display, scalled_width, scalled_height) != NO_ERROR)
    {
        fprintf(stderr, "Error capturing screen\n");
        size = http_header(buf, header_internal_error, "text/plain", 0);
        write(fd, buf, size);
        return;
    }

    stride = screenshot.getStride();
    format = screenshot.getFormat();
    bpp = bytesPerPixel(format);
    bmpConfig = flinger2skia(format);
    b.setConfig(bmpConfig, scalled_width, scalled_height, stride * bpp);
    b.setPixels((void *)screenshot.getPixels());
    SkImageEncoder::EncodeStream(&stream, b,
                                 SkImageEncoder::kWEBP_Type,
                                 SkImageEncoder::kDefaultQuality);
    streamData = stream.copyToData();
    size = http_header(buf, header_ok, "image/webp", streamData->size());
    write(fd, buf, size);
    printf(">>>>\n%s-----------\n", buf);
    write(fd, streamData->data(), streamData->size());
    streamData->unref();
    stream.reset();
}

static void handle_connection(int fd)
{
    int client_fd, bufSize = 65535;
    struct sockaddr_in client;
    socklen_t clientLen;
    DisplayInfo mainDpyInfo;
    char *req_version, *method, *path;

    ProcessState::self()->startThreadPool();

    display = SurfaceComposerClient::getBuiltInDisplay(ISurfaceComposer::eDisplayIdMain);
    servicemanager = defaultServiceManager();

    if (SurfaceComposerClient::getDisplayInfo(display, &mainDpyInfo) != NO_ERROR)
    {
        fprintf(stderr, "ERROR: unable to get display characteristics\n");
        return;
    }

    printf("Main display is %dx%d @%.2ffps (orientation=%u)\n",
           mainDpyInfo.w, mainDpyInfo.h, mainDpyInfo.fps,
           mainDpyInfo.orientation);

    scalled_width = mainDpyInfo.w / 2;
    scalled_height = mainDpyInfo.h / 2;

    while (1)
    {
        clientLen = sizeof(client);
        memset(&client, 0, clientLen);
        printf("Waiting connection... ");
        client_fd = accept(fd, (struct sockaddr *)&client, &clientLen);
        if (client_fd < 0)
        {
            printf("Fail accepting client\n");
            continue;
        }
        size = recv(client_fd, buf, bufSize, 0);
        printf("%d bytes\n", size);
        if (size < 0)
        {
            printf("Ignore recv %d bytes\n", size);
            continue;
        }
        buf[size] = 0;
        method = strtok(buf, " ");
        path = strtok(NULL, " ");
        req_version = strtok(NULL, "\r\n");
        printf("<<<<\n%s %s\n", method, path);

        if (strcmp(method, "GET") != 0)
        {
            size = http_header(buf, header_not_allowed, NULL, 0);
            write(client_fd, buf, size);
        }

        if (strcmp(path, "/dump") == 0)
        {
            send_dump(client_fd);
        }
        else if (strcmp(path, "/") == 0)
        {
            send_screenshot(client_fd);
        }
        else
        {
            size = http_header(buf, header_not_found, NULL, 0);
            write(client_fd, buf, size);
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
    pthread_t th;

    if (fd == -1)
    {
        fprintf(stderr, "Fail creating socket\n");
        return 1;
    }
    if (pthread_create(&th, NULL, sleeping_check_thread, NULL) != 0)
    {
        printf("Fail starting sleeping checker thread\n");
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