#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <binder/ProcessState.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include <ui/PixelFormat.h>
#include <SkImageEncoder.h>
#include <SkBitmap.h>
#include <SkData.h>
#include <SkStream.h>
#include "screen.h"
using namespace android;
static uint32_t DEFAULT_DISPLAY_ID = ISurfaceComposer::eDisplayIdMain;
// static void usage(const char* pname)
// {
//     fprintf(stderr,
//             "usage: %s [-hp] [-d display-id] [FILENAME]\n"
//             "   -h: this message\n"
//             "   -p: save the file as a png.\n"
//             "   -d: specify the display id to capture, default %d.\n"
//             "If FILENAME ends with .png it will be saved as a png.\n"
//             "If FILENAME is not given, the results will be printed to stdout.\n",
//             pname, DEFAULT_DISPLAY_ID
//     );
// }
static SkBitmap::Config flinger2skia(PixelFormat f)
{
    switch (f) {
        case PIXEL_FORMAT_RGB_565:
            return SkBitmap::kRGB_565_Config;
        default:
            return SkBitmap::kARGB_8888_Config;
    }
}
static status_t vinfoToPixelFormat(const fb_var_screeninfo& vinfo,
        uint32_t* bytespp, uint32_t* f)
{
    switch (vinfo.bits_per_pixel) {
        case 16:
            *f = PIXEL_FORMAT_RGB_565;
            *bytespp = 2;
            break;
        case 24:
            *f = PIXEL_FORMAT_RGB_888;
            *bytespp = 3;
            break;
        case 32:
            // TODO: do better decoding of vinfo here
            *f = PIXEL_FORMAT_RGBX_8888;
            *bytespp = 4;
            break;
        default:
            return BAD_VALUE;
    }
    return NO_ERROR;
}
int screen_grab(int fd)
{
    ProcessState::self()->startThreadPool();
    int32_t displayId = DEFAULT_DISPLAY_ID;
    // const char* pname = argv[0];
    // bool png = false;
    // int c;
    // while ((c = getopt(argc, argv, "phd:")) != -1) {
    //     switch (c) {
    //         case 'p':
    //             png = true;
    //             break;
    //         case 'd':
    //             displayId = atoi(optarg);
    //             break;
    //         case '?':
    //         case 'h':
    //             usage(pname);
    //             return 1;
    //     }
    // }
    // argc -= optind;
    // argv += optind;
    // int fd = -1;
    // if (argc == 0) {
    //     fd = dup(STDOUT_FILENO);
    // } else if (argc == 1) {
    //     const char* fn = argv[0];
    //     fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    //     if (fd == -1) {
    //         fprintf(stderr, "Error opening file: %s (%s)\n", fn, strerror(errno));
    //         return 1;
    //     }
    //     const int len = strlen(fn);
    //     if (len >= 4 && 0 == strcmp(fn+len-4, ".png")) {
    //         png = true;
    //     }
    // }
    
    // if (fd == -1) {
    //     usage(pname);
    //     return 1;
    // }
    void const* mapbase = MAP_FAILED;
    ssize_t mapsize = -1;
    void const* base = 0;
    uint32_t w, s, h, f;
    size_t size = 0;
    ScreenshotClient screenshot;
    sp<IBinder> display = SurfaceComposerClient::getBuiltInDisplay(displayId);
    if (display != NULL && screenshot.update(display) == NO_ERROR) {
        base = screenshot.getPixels();
        w = screenshot.getWidth();
        h = screenshot.getHeight();
        s = screenshot.getStride();
        f = screenshot.getFormat();
        size = screenshot.getSize();
    } else {
        fprintf(stderr, "Can't open display\n");
        return 1;
    }
    SkBitmap b;
    b.setConfig(flinger2skia(f), w, h, s*bytesPerPixel(f));
    b.setPixels((void*)base);
    SkDynamicMemoryWStream stream;
    SkImageEncoder::EncodeStream(&stream, b,
            SkImageEncoder::kPNG_Type, SkImageEncoder::kDefaultQuality);
    SkData* streamData = stream.copyToData();
    write(fd, streamData->data(), streamData->size());
    streamData->unref();
    //close(fd);
    if (mapbase != MAP_FAILED) {
        munmap((void *)mapbase, mapsize);
    }
    return 0;
}