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

static inline uint32_t pack_rgb(const struct fb_var_screeninfo* v, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t rv = (v->red.length   ? ((uint32_t)r * ((1u << v->red.length)   - 1) + 127) / 255 : 0);
    uint32_t gv = (v->green.length ? ((uint32_t)g * ((1u << v->green.length) - 1) + 127) / 255 : 0);
    uint32_t bv = (v->blue.length  ? ((uint32_t)b * ((1u << v->blue.length)  - 1) + 127) / 255 : 0);
    return (rv << v->red.offset) | (gv << v->green.offset) | (bv << v->blue.offset);
}

static inline void write_pixel(uint8_t* p, uint32_t bpp, uint32_t pix)
{
    uint32_t bytes = bpp / 8;
    for (uint32_t i = 0; i < bytes; i++) {
        p[i] = (uint8_t)(pix >> (8u * i));
    }
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

    if (!(vinfo.bits_per_pixel == 24 || vinfo.bits_per_pixel == 32)) {
        panic("Unsupported BPP");
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

    while (1) {
        for (uint32_t y = 0; y < vinfo.yres; ++y) {
            uint8_t* row = fb + (size_t)y * finfo.line_length;
            for (size_t x = 0; x < vinfo.xres; ++x) {
                uint8_t r = 0xFF * x / vinfo.xres;
                uint8_t g = 0xFF * y / vinfo.yres;
                uint32_t color = pack_rgb(&vinfo, r, g, 0x00);
                size_t bpp = vinfo.bits_per_pixel;
                size_t bypp = bpp / 8;
                write_pixel(&row[(size_t)x * bypp], bpp, color);
            }
        }
        usleep(1000 * 1000 * 1);
    }
}
