#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

char content[100];

void *connection_handler(void *fd);

struct con_info {
    int connection_fd;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
};
 

char *bind_ip = "127.0.0.1"; // INADDR_LOOPBACK

void usage() {
    printf("./tool ${bind_port}\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return -1;
    }

    int port = atoi(argv[1]);
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
    result = listen(bfd, 1);
    if (result == -1) {
        perror("fail to listen socket fd\n");
        exit(-1);
    }

    printf("successfully set socket listened !!!\n");

    // printf("goto sleep for 30 seconds!!!\n");
    // sleep(30);
    // printf("wake up!!!\n");
    
    pthread_t thread_idt;

    while (1) {
        struct con_info *connection_info = malloc(sizeof(struct con_info));
        connection_info->addr_len = sizeof(struct sockaddr_in);
        connection_info->connection_fd = accept(
                bfd,
                (struct sockaddr *)&(connection_info->client_addr),
                &(connection_info->addr_len)
                );
        if (connection_info->connection_fd == -1) {
            perror("fail to accept socket fd\n");
            continue;
        }
        printf(
                "go handle connection from %s:%d\n",
                inet_ntoa(connection_info->client_addr.sin_addr),
                ntohs(connection_info->client_addr.sin_port)
                );

        result = pthread_create(
                &thread_idt,
                NULL,
                connection_handler,
                connection_info
                );
        if (result != 0) {
            printf(
                    "erro to create thread to handle addr: %s port: %d.\n",
                    inet_ntoa(connection_info->client_addr.sin_addr),
                    ntohs(connection_info->client_addr.sin_port)
                  );
            close(connection_info->connection_fd);
        }
    }

    printf("exiting...\n");
    return 0;
}

void *connection_handler(void *info) {
    pthread_t thread_id = pthread_self();
    printf("[%ld] start process ......\n", thread_id);
    struct con_info *connection_info = (struct con_info *)info;

    memset((void*)content, 0x0, 100);
    // recv data
    ssize_t recv_len = recv(connection_info->connection_fd, content, 100, 0);
    printf("[%ld] recv len: %d, content: %s.\n", thread_id, recv_len, content);

    printf("[%ld] ok, simply close it.\n", thread_id);
    shutdown(connection_info->connection_fd, SHUT_WR);
    printf("[%ld] stop process ......\n", thread_id);
}
