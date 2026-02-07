#include <errno.h>
#include <fcntl.h>
// #include <linux/fb.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <sys/uio.h>
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

    const char* msg = "[rlwsd] HELLO!\n";
    write(1, msg, __builtin_strlen(msg));
    for (;;)
    {
        pause();
    }
}
