#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stdio.h>

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s source destination\n", argv[0]);
        return 64;
    }

    if (renameatx_np(AT_FDCWD, argv[1], AT_FDCWD, argv[2], RENAME_EXCL) != 0) {
        fprintf(stderr, "rename-excl: %s -> %s: %s\n", argv[1], argv[2], strerror(errno));
        return 73;
    }
    return 0;
}
