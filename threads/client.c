#include <stdio.h>

#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

char content[100] = {"\0"};

struct con_info {
    int connection_fd;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
};
 

char *bind_ip = "0.0.0.0"; // INADDR_LOOPBACK
// char *bind_ip = "127.0.0.1"; // INADDR_LOOPBACK
// char *server_ip = "127.0.0.1"; // INADDR_LOOPBACK
char *server_ip = "192.168.9.8"; // INADDR_LOOPBACK

void usage() {
    printf("./tool ${server_port} ${bind_port}\n");
}

int main(int argc, char **argv) {
    if (argc < 3) {
        usage();
        return -1;
    }

    int server_port = atoi(argv[1]);
    int port = atoi(argv[2]);
    int bfd = socket(AF_INET, SOCK_STREAM, 0);
    if (bfd <= 0) {
        perror("fail to create socket fd\n");
        exit(-1);
    }

    struct sockaddr_in bind_addr;
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(port);
    int result = inet_aton(bind_ip, &bind_addr.sin_addr);
    if (result == 0) {
        err(-1, "fail to convert addr %s with inet_aton\n", bind_ip);
    }

    result = bind(
            bfd,
            (struct sockaddr *)&bind_addr,
            sizeof(struct sockaddr_in)
            );
    if (result == -1) {
        perror("fail to bind socket fd\n");
        exit(-1);
    }

    // printf("goto sleep for 30 seconds!!!\n");
    // sleep(30);
    // printf("wake up!!!\n");

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    result = inet_aton(server_ip, &server_addr.sin_addr);
    if (result == 0) {
        err(-1, "fail to convert addr %s with inet_aton\n", server_ip);
    }

    result = connect(
            bfd,
            (struct sockaddr *)&server_addr,
            (socklen_t)sizeof(struct sockaddr_in)
            ); 
    if (result == -1) {
        perror("fail to connect to server");
        exit(-1);
    }
    printf("successfully connect socket !!!\n");
    content[0] = 'a';
    content[1] = 'b';
    content[2] = 'c';
    ssize_t send_len = send(bfd, content, 3, 0);
    printf("send length: %d.\n", send_len);


    // sleep(60);
    shutdown(bfd, SHUT_WR);

    printf("exiting...\n");
    return 0;
}
