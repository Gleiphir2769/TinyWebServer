//
// Created by daqige on 2021/1/8.
//
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include "threadpool.h"

class test_worker {
public:
    test_worker(char* c) {
        content = c;
    };
    ~test_worker() {};
    void process() {
        printf("Server start working! Process the content: %s\n", content);
        sleep(2);
        printf("Server process finished\n");
    }
    void init(int epollfd, int connfd, int pid, struct sockaddr_in addr) {
        printf("Process server %d initialized!\n", pid);
    }

private:
    char* content;
};

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epoll_fd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// restart 用来标志是否继续被信号打断的系统调用
void addsig(int sig, void(*handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

bool stop = false;

int main(int argc, char* argv[]) {
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi( argv[2] );

    int ret = 0;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );

    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );

    ret = listen( listenfd, 5 );
    assert( ret != -1 );

    // 初始化epoll
    int epollfd;
    epoll_event events[1024];
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);

    threadpool<test_worker>* pool = new threadpool<test_worker>;

    while (!stop) {
        int number = epoll_wait(epollfd, events, 1024, -1);
        if (number < 0 && errno != EINTR) {
            printf("Epoll failed\n");
            throw std::exception();
        }
        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                struct sockaddr_in client_addr;
                socklen_t client_addrlen = sizeof(client_addr);
                int connfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addrlen);
                if (connfd < 0) {
                    printf("Connect failed\n");
                    continue;
                }
                addfd(epollfd, connfd);
            }
            else if (events[i].events & EPOLLIN) {
                char buffer[1024];
                memset(buffer, '\0', 1024);
                ret = recv(sockfd, buffer, 1024, 0);
                if (ret < 0)
                {
                    if (errno != EAGAIN) {
                        close(sockfd);
                        printf("Read a client failed\n");
                    }
                }
                else if (ret == 0) {
                    continue;
                }
                else {
                    test_worker* request = new test_worker(buffer);
                    pool->append(request);
                }
            }
        }
    }
}
