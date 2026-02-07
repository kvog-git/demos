#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <sys/wait.h>

static void write_str(int fd, const char* str)
{
    write(fd, str, __builtin_strlen(str));
}

static void error(const char* message)
{
    // Attempt to spew to kernel log
    int kmfd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
    if (kmfd >= 0) {
        write_str(kmfd, message);
        close(kmfd);
    }

    _exit(1); // Kernel panic
}
#define ERROR(message) error("[rlinit] " message)

static bool mount_required(const char* src, const char* tgt, const char* fs)
{
    // EBUSY means already mounted, which is ok
    return mount(src, tgt, fs, 0, 0) >= 0 || errno == EBUSY;
}

int main(void)
{
#if 0
    // First, spew a message to tty to signal we at least made it this far
    {
        const char* hello = "rlinit: Initialization started\n";
        write(1, hello, __builtin_strlen(hello));
    }
#endif

    // Mount /dev first, as /dev/kmsg is useful for debugging errors
    // @@ include errno in error messages
    if (!mount_required("devtmpfs", "/dev", "devtmpfs")) {
        ERROR("Failed to mount /dev\n");
    }
    if (!mount_required("proc", "/proc", "proc")) {
        ERROR("Failed to mount /proc\n");
    }
    if (!mount_required("sysfs", "/sys", "sysfs")) {
        ERROR("Failed to mount /sys\n");
    }

    // Write to kernel log
    {
        int kmfd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
        if (kmfd >= 0) {
            const char* hello = "<6>Rushmore Linux - Nothing is beyond our reach.\n";
            write(kmfd, hello, __builtin_strlen(hello));
            close(kmfd);
        }
    }

    // pid_t pid = fork();
    // if (pid == 0) {
    //     char *argv[] = { (char*)"/rlwsd", 0 };
    //     execv(argv[0], argv);
    //     _exit(127);
    // }

    // int st = 0;
    // waitpid(pid, &st, 0);
    // write(1, "rlinit: rlwsd exited\n", 21);

    for (;;) pause();
}
