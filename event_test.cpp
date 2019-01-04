#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <string.h>

#include <signal.h> // we have to ignore pipe error signal

#include <sys/types.h>
#include <sys/socket.h>
// port/addr translate
#include <arpa/inet.h>
#include <sys/epoll.h>

#include <iostream>
#include <map>
#include <set>

#define ERROR_EXIT_NO 255
#define ERROR_EXIT(...) error_at_line(ERROR_EXIT_NO, errno, __FILE__, __LINE__, __VA_ARGS__)
#define ERROR_NO_EXIT(...) error_at_line(0, errno, __FILE__, __LINE__, __VA_ARGS__)

#define PROXY_LISTEN_PORT 3344

#define BACKLOG 7
#define TRUE 1
#define FALSE 0

#define INIT_CAPACITY 1024
char tmp_content[INIT_CAPACITY];

const int MAX_FD_COUNT = 7;

struct fd_context {
    int fd;
    struct sockaddr_in client_addr;
};

int main() {
    if ( signal(SIGPIPE, SIG_IGN) == SIG_ERR ) {
        ERROR_EXIT("fail to ignore signal pipe");
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
    if (listen_fd == -1) {
        ERROR_EXIT("fail to create listen fd");
    } else {
        std::cout << "create listen_fd: " << listen_fd << std::endl;
    }

    int o = 1;
    if ( setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &o, sizeof(o)) == -1 ) {
        ERROR_EXIT("fail to set listen_fd SO_REUSEADDR");
    }

    struct sockaddr_in listen_addr; // use man 7 ip to find out how to set sockaddr_in
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(PROXY_LISTEN_PORT);
    listen_addr.sin_addr.s_addr = INADDR_ANY;

    if ( bind(listen_fd, (struct sockaddr *)&listen_addr, (socklen_t)sizeof(listen_addr)) != 0 ) {
        ERROR_EXIT("fail to bind listen fd");
    }

    if ( listen(listen_fd, BACKLOG) != 0 ) {
        ERROR_EXIT("fail to listen fd");
    }

    int epoll_fd = epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        ERROR_EXIT("fail to make epoll fd");
    } else {
        std::cout << "create epoll_fd: " << epoll_fd << std::endl;
    }

    struct epoll_event lev;
    lev.events = EPOLLIN;
    lev.data.fd = listen_fd;
    if ( epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &lev) == -1 ) {
        ERROR_EXIT("fail to add listen_fd to epoll");
    }
    int current_fd_count = 1; // only listen fd is waited

    int error_code = 0;
    int error_code_len = 0;
    struct epoll_event events[MAX_FD_COUNT]; // in && out and listen fd
    int ready_count = 0;
    std::map<int, struct fd_context *> fd_contexts;

    while (TRUE) {
        std::cout << "gonna epoll_wait" << std::endl;
        ready_count = epoll_wait(epoll_fd, events, current_fd_count, -1/*timeout*/); // 100 means 100 milliseconds
        if (ready_count == -1) {
            if (errno != EINTR) {
                ERROR_EXIT("epoll_wait error");
            } else {
                ERROR_NO_EXIT("epoll_wait interrupted");
                continue;
            }
        }

        if (ready_count == 0) {
            std::cout << "no fd ready, continue to next epoll_wait" << std::endl;
            continue;
        }

        std::set<int> to_remove_fd_sets;
        for (int j=0; j!=ready_count; j++) {
            struct epoll_event event = events[j];

            // accept on listen_fd
            if (event.data.fd == listen_fd) {
                if ( (event.events & EPOLLERR) != 0 ) {
                    error_code_len = sizeof(error_code);
                    if ( getsockopt(listen_fd, SOL_SOCKET, SO_ERROR, &error_code, (socklen_t *)&error_code_len) == -1 ) {
                        ERROR_EXIT("fail to get listen_fd error flag");
                    }
                    // print error message and exit 
                    std::cerr << strerror(error_code) << std::endl;
                    exit(ERROR_EXIT_NO);
                } 

                while (TRUE) {
                    struct sockaddr_in client_addr;
                    socklen_t addr_len = (socklen_t)sizeof(struct sockaddr_in);

                    int fd = accept4(listen_fd, (struct sockaddr *)&client_addr, &addr_len, SOCK_NONBLOCK);
                    if (fd == -1) {
                        if (errno == EWOULDBLOCK || errno == EAGAIN) {
                            break;
                        } else {
                            ERROR_EXIT("fail to accept4");
                        }
                    }

                    if ( current_fd_count >= MAX_FD_COUNT) {
                        std::cerr << "will overflow max concurrent connection, skip accept" << std::endl;
                        close(fd); // we should accept and close, otherwise listen_fd is always EPOLLIN
                        continue;
                    }

                    struct fd_context *in_context = new(struct fd_context);
                    memset(in_context, 0x0, sizeof(struct fd_context));
                    in_context->fd = fd;
                    in_context->client_addr = client_addr;
                    fd_contexts[fd] = in_context;

                    struct epoll_event iev;
                    iev.events = EPOLLIN | EPOLLET; // we do not need to write
                    iev.data.fd = fd;
                    if ( epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &iev) == -1 ) {
                        ERROR_EXIT("fail to add incoming fd to epoll");
                    }
                    current_fd_count += 1;

                    char addr[INET6_ADDRSTRLEN] = {0};
                    std::cout << "incoming connect, fd: " << fd;
                    std::cout << ", client addr: " << inet_ntop(
                            AF_INET, &client_addr.sin_addr, addr, (socklen_t)(INET6_ADDRSTRLEN-1));
                    std::cout << ", client port: " << ntohs(client_addr.sin_port) << std::endl;
                }

            } else { // handler for listen_fd ends, handler for others starts
                int fd = event.data.fd;
                struct fd_context *context = fd_contexts[fd];
                if (context == NULL) {
                    std::cerr << "unknown event.data.fd: " << fd << std::endl;
                    exit(ERROR_EXIT_NO);
                }

                if ( (event.events & (EPOLLERR|EPOLLHUP)) != 0 ) {
                    std::cout << "error found in fd: " << fd << std::endl;
                    if ( (event.events & EPOLLERR) != 0 ) {
                        error_code_len = sizeof(error_code);
                        if ( getsockopt(fd, SOL_SOCKET, SO_ERROR, &error_code, (socklen_t *)&error_code_len) == -1 ) {
                            ERROR_EXIT("fail to get fd error flag");
                        }
                        // print error message and exit 
                        std::cerr << "fd epollerr: " << strerror(error_code) << ", fd: " << fd << std::endl;
                    } 

                    if ( (event.events & EPOLLHUP) != 0 ) {
                        std::cerr << "fd hang up" << std::endl;
                    }

                    to_remove_fd_sets.insert(fd);
                } 

                if ( (event.events & EPOLLIN) != 0 ) { // read into content
                    std::cout << "data arrived from fd: " << fd << std::endl;

                    while (TRUE) {
                        int byte_count = read(
                                context->fd, (void*)tmp_content, INIT_CAPACITY);
                        if (byte_count == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;
                            } else if (errno == EINTR) {
                                continue;
                            } else {
                                ERROR_NO_EXIT("fail to read from fd");
                                to_remove_fd_sets.insert(fd);
                                break;
                            }

                        } else if (byte_count > 0) {
                            std::cout << "read from byte count: " << byte_count << " from fd: " << fd << std::endl;

                        } else if (byte_count == 0){
                            std::cout << "seems remote close write, fd: " << fd << std::endl;
                            to_remove_fd_sets.insert(fd);
                            break;

                        } else {
                            ERROR_EXIT("should never be here");
                        }

                        sleep(3);
                    }
                }
            }
        } // for loop with epoll events

        for (std::set<int>::iterator it = to_remove_fd_sets.begin(); it != to_remove_fd_sets.end(); it ++) { // actual delete
            std::cout << "remove fd: " << *it << std::endl;
            if ( fd_contexts.find(*it) == fd_contexts.end() ) {
                std::cerr << "fd not found in fd_contexts, fd: " << *it << std::endl;
                exit(ERROR_EXIT_NO);
            }

            struct fd_context *context = fd_contexts[*it];
            if ( epoll_ctl(epoll_fd, EPOLL_CTL_DEL, context->fd, NULL) == -1 ) {
                ERROR_EXIT("fail to delete fd of epoll");
            }
            fd_contexts.erase(context->fd);
            close(context->fd);
            delete(context);
            current_fd_count -= 1;
        }

    } // while (TRUE)

EXIT: // should never be here because we exit directly on error
    close(epoll_fd);
    close(listen_fd);
    return 0;
}
