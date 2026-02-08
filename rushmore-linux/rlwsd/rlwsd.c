#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

static const char* tmp_fmt(const char* fmt, ...)
{
    static char msg[1024] = { 0 };
    va_list va; va_start(va, fmt);
    vsnprintf(msg, sizeof(msg), fmt, va);
    va_end(va);
    return msg;
}

static void spew(const char* message)
{
    // Kernel log is available after init
    int kmfd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);

    struct iovec msgvec[] = {
#define STRVEC(str) { .iov_base = (void*)(str), .iov_len = __builtin_strlen(str) }
        STRVEC("rlwsd: "),
        STRVEC(message),
        STRVEC("\n")
#undef STRVEC
    };
    writev(kmfd, msgvec, sizeof(msgvec) / sizeof(*msgvec));

    close(kmfd);
}

static void panic(const char* message)
{
    spew(message);
    _exit(1); // Triggers a kernel panic since we're PID 1
}

int main(void)
{
    int fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        panic(tmp_fmt("Failed to open /dev/fb0: %s", strerror(errno)));
    }

    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) != 0) {
        panic(tmp_fmt("FBIOGET_FSCREENINFO: %s", strerror(errno)));
    }
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) != 0) {
        panic(tmp_fmt("FBIOGET_VSCREENINFO: %s", strerror(errno)));
    }

    spew(tmp_fmt("fb0: %ux%u bpp=%u line_length=%u visual=%u",
        vinfo.xres, vinfo.yres, vinfo.bits_per_pixel,
        finfo.line_length, finfo.visual));
    spew(tmp_fmt("fb0: R %u/%u G %u/%u B %u/%u",
        vinfo.red.offset, vinfo.red.length,
        vinfo.green.offset, vinfo.green.length,
        vinfo.blue.offset, vinfo.blue.length));

    if (vinfo.bits_per_pixel != 24) {
        panic("BPP must be 24");
    }

    // Disable TTY
    int tty = open("/dev/tty0", O_RDWR);
    if (tty < 0) {
        tty = open("/dev/console", O_RDWR);
        if (tty < 0) {
            panic(tmp_fmt("Couldn't open tty: %s", strerror(errno)));
        }
    }
    if (ioctl(tty, KDSETMODE, KD_GRAPHICS) != 0) {
        panic(tmp_fmt("Failed to set tty mode to KD_GRAPHICS: %s", strerror(errno)));
    }
    close(tty);

    size_t fb_size = (size_t)finfo.line_length * vinfo.yres;
    uint8_t* fb = (uint8_t*)mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) {
        panic(tmp_fmt("mmap failed: %s", strerror(errno)));
    }

#if 1
    for (uint32_t y = 0; y < vinfo.yres; ++y) {
        uint8_t* row = fb + (size_t)y * finfo.line_length;
        memset(row, 0x00, finfo.line_length);
    }
#endif

    uint8_t lum = 0;
    uint32_t y = 0;
    float then = 0.0f;

    for (;;)
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        float now = ts.tv_sec + (ts.tv_nsec / 1e9f);
        if ((now - then) >= (1.0f / 60.0f)) {
            then = now;

            uint32_t color = (uint32_t)lum << 24 | (uint32_t)lum << 16 | (uint32_t)lum << 8 | lum;
            uint8_t* row = fb + (size_t)y * finfo.line_length;
            for (size_t x = 0; x < vinfo.xres; ++x) {
                size_t bpp = vinfo.bits_per_pixel / 8;
                memcpy(&row[(x * bpp) + x], &color, 4);
            }

            lum = (lum + 7) % 255;
            y = (y + 1) % (vinfo.yres - 1);
        }
    }
}
