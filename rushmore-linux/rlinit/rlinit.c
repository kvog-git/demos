#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <stdio.h>

// ref: https://git.suckless.org/sinit/file/sinit.c.html

static void panic(const char* message)
{
    // Attempt to spew to kernel log
    int kmfd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
    int fd = (kmfd >= 0) ? kmfd : 2;

    struct iovec msgvec[] = {
#define STRVEC(str) { .iov_base = (void*)(str), .iov_len = __builtin_strlen(str) }
        STRVEC("rlinit: "),
        STRVEC(message),
        STRVEC("\n")
#undef STRVEC
    };
    writev(fd, msgvec, sizeof(msgvec) / sizeof(*msgvec));

    if (kmfd >= 0) {
        close(kmfd);
    }

    _exit(1); // Triggers a kernel panic since we're PID 1
}

static bool mkdir_if_missing(const char* path, mode_t mode)
{
    return mkdir(path, mode) == 0 || errno == EEXIST;
}

int main(void)
{
    if (getpid() != 1) {
        panic("PID is not 1. Aborting.");
    }

    // Set up filesystem in descending priority
    //
    // @@ include errno in error messages

    if (!mkdir_if_missing("/dev", 0755)) {
        panic("Failed to create /dev");
    }
    if (mount("devtmpfs", "/dev", "devtmpfs", MS_NOSUID, 0) < 0) {
        panic("Failed to mount /dev");
    }

    if (!mkdir_if_missing("/proc", 0555)) {
        panic("Failed to create /proc");
    }
    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, 0) < 0) {
        panic("Failed to mount /proc");
    }

    if (!mkdir_if_missing("/sys", 0555)) {
        panic("Failed to create /sys");
    }
    if (mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV, 0) < 0) {
        panic("Failed to mount /sys");
    }

    if (!mkdir_if_missing("/tmp", 0777)) {
        panic("Failed to create /tmp");
    }
    chmod("/tmp", 01777);

    // Test write to kernel log
    {
        int kmfd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
        if (kmfd >= 0) {
            const char* hello = "<6>Rushmore Linux - Nothing is beyond our reach.\n";
            write(kmfd, hello, __builtin_strlen(hello));
            close(kmfd);
        }
    }

    pid_t child = fork();
    switch(child) {
    case 0: {
        char* const argv[] = { "/rltest", 0 };
        execv(argv[0], argv);
        panic("execvp failed");
    } break;
    case -1: {
        panic("Failed to fork()");
    } break;
    }

    // @@ reap child processes

    for (;;) pause();
}
