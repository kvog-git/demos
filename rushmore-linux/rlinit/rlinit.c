#include <unistd.h>
#include <sys/mount.h>
#include <sys/wait.h>

int main(void)
{
    mount("proc", "/proc", "proc", 0, 0);
    mount("sysfs", "/sys", "sysfs", 0, 0);
    mount("devtmpfs", "/dev", "devtmpfs", 0, 0);

    write(1, "rlinit: starting rlwsd\n", 23);

    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = { (char*)"/rlwsd", 0 };
        execv(argv[0], argv);
        _exit(127);
    }

    int st = 0;
    waitpid(pid, &st, 0);
    write(1, "rlinit: rlwsd exited\n", 21);
    for (;;) pause();
}
