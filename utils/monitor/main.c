#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>

typedef struct {
    int fd;
    char buf[2048];
} app_s;

static app_s a;

const char* format_ip(uint32_t ip)
{
    static char buf[16];
    unsigned char b[] = { ip, ip >> 8, ip >> 16, ip >> 24};

    snprintf(buf, sizeof(buf), "%d.%d.%d.%d", b[0], b[1], b[2], b[3]);
    return buf;
}

int main(int argc, char** argv)
{
    // Create UDP Socket
    if ((a.fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Can't create socket");
        goto clean_up;
    }

    // Configure socket
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr = { .s_addr = htonl(INADDR_ANY) },
        .sin_port = htons(20000),
    };

    // Bind socket
    if ((bind(a.fd, (struct sockaddr*)&addr, sizeof(addr))) == -1) {
        perror("Can't bind");
        goto clean_up;
    }

    while (true) {
        struct sockaddr_in src;
        socklen_t len = sizeof(src);

        int cnt = recvfrom(a.fd, 
                           a.buf, sizeof(a.buf) - 1, 
                           0, 
                           (struct sockaddr*)&src, &len);

        if (cnt < 1) {
            perror("Recv error");
            goto clean_up;
        }

        a.buf[cnt] = 0;
        printf("%s: %s\n", format_ip(src.sin_addr.s_addr), a.buf);
    }

    clean_up:
        close(a.fd);
        return 0;
}