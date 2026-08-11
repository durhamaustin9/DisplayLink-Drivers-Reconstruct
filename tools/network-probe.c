#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 3;
    }

    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl");
        close(fd);
        return 3;
    }

    /* RFC 5737 TEST-NET-2: reserved for documentation and examples. */
    struct sockaddr_in target = {0};
    target.sin_family = AF_INET;
    target.sin_port = htons(9);
    inet_pton(AF_INET, "198.51.100.1", &target.sin_addr);

    errno = 0;
    int result = connect(fd, (struct sockaddr *)&target, sizeof(target));
    int saved_errno = errno;
    close(fd);

    printf("connect_result=%d errno=%d (%s)\n", result, saved_errno,
           strerror(saved_errno));
    return saved_errno == EPERM ? 2 : 0;
}
