//
// Created by daqige on 2021/1/2.
//
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include "timer_wheel.h"

#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5

static int pipefd[2];
static time_wheel timer_wheel;
static int epollfd = 0;

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 信号处理在这里就是将信号发送给管道
void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*) &msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void timer_handler() {
    // 定时到，进到定时处理函数（即tick函数）
    timer_wheel.tick();
    // 重新设置定时，因为alarm只会触发一次SIGALRM
    alarm(TIMESLOT);
}

// 定时器回调函数，定时器回调函数指针是被注册给定时器链表上的定时器对象的
void cb_func(client_data* user_data) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    printf("close fd %d\n", user_data->sockfd);
}

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

    epoll_event events[MAX_EVENT_NUMBER];
    int epoll_fd = epoll_create(5);
    assert(epoll_fd != -1);
    addfd(epoll_fd, listenfd);

    // 这里实际上是创建一组匿名socket到pipefd上，也就是说这里的pipefd实际上是socket
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    // 设置写端为非阻塞，将读端加入到epoll的事件表中
    setnonblocking(pipefd[1]);
    addfd(epoll_fd, pipefd[0]);

    addsig(SIGALRM);
    addsig(SIGTERM);
    bool stop_server = false;

    client_data* users = new client_data[FD_LIMIT];
    bool timeout = false;
    alarm(TIMESLOT);

    while (!stop_server) {
        int number = epoll_wait(epoll_fd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            printf("epoll failed!\n");
            break;
        }

        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                struct sockaddr_in client_address;
                socklen_t client_len = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_len);
                addfd(epoll_fd, connfd);
                users[connfd].address = client_address;
                users[connfd].sockfd = connfd;
                /*
                 * 创建定时器，绑定到用户数据上
                 */
                time_t cur = time(NULL);
                tw_timer* timer = timer_wheel.add_timer(3 * TIMESLOT);
                timer->cb_func = cb_func;
                timer->user_data = &users[connfd];
                users[connfd].timer = timer;
            }
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) {
                // 信号可读
                int sig;
                // 信号数据缓冲区
                char signals[1024];
                ret = recv(sockfd, signals, sizeof(signals), 0);
                if (ret == -1) {
                    continue;
                }
                else if (ret == 0) {
                    continue;
                }
                else {
                    for (int j = 0; j < ret; ++j) {
                        switch (signals[i]) {
                            case SIGALRM:
                            {
                                timeout = true;
                                break;
                            }
                            case SIGTERM:
                            {
                                stop_server = true;
                            }
                        }
                    }
                }
            }
            else if (events[i].events & EPOLLIN) {
                memset(users[sockfd].buf, '\0', BUFFER_SIZE);
                ret = recv(sockfd, users[sockfd].buf, BUFFER_SIZE-1, 0);
                printf("get %d bytes of client data %s, from %d\n", ret, users[i].buf, sockfd);
                tw_timer* timer = users[sockfd].timer;
                if (ret < 0) {
                    if (errno != EAGAIN) {
                        printf("Something Wrong\n");
                        cb_func(&users[sockfd]);
                        if (timer) {
                            timer_wheel.del_timer(timer);
                        }
                    }
                }
                else if (ret == 0) {
                    printf("Connection has been closed by opposite port!\n");
                    cb_func(&users[sockfd]);
                    if (timer) {
                        timer_wheel.del_timer(timer);

                    }
                }
//                else {
//                    if (timer) {
//                        time_t cur = time(NULL);
//                        timer->expire = cur + 3 * TIMESLOT;
//                        printf("adjust timer once\n");
//                        timer_lst.adjust_timer(timer);
//                    }
//                    else {
//                        printf("timer is none\n");
//                        break;
//                    }
//                }
            }
        }

        if( timeout )
        {
            timer_handler();
            timeout = false;
        }
    }
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    delete [] users;
    return 0;
}





