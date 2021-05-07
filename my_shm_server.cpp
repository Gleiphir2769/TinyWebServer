//
// Created by daqige on 2021/1/4.
//

#ifndef TINYWEBSERVER_MY_SHM_SERVER_H
#define TINYWEBSERVER_MY_SHM_SERVER_H

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


#define USER_LIMIT 5
#define BUFFER_SIZE 1024
#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define PROCESS_LIMIT 131070

struct client_data {
    sockaddr_in address;
    int connfd;
    pid_t pid;
    int pipefd[2];
};

static const char *shm_name = "/my_shm";
int sig_pipefd[2];
int epollfd;
int listenfd;
int shmfd;
char *share_mem = 0;
client_data *users = 0;
int *sub_process = 0;
int user_count = 0;
bool stop_child = false;

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

void sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char *) &msg, 1, 0);
    errno = save_errno;
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

void del_resource() {
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(listenfd);
    close(epollfd);
    shm_unlink(shm_name);
    delete[] users;
    delete[] sub_process;
}

/*
 * 形参sig的作用是与信号注册的信号处理函数形式一致，实际上本代码中信号处理函数有两个，仔细思考回调函数的意义
 */
void child_term_handler(int sig) {
    stop_child = true;
}

/*
 * run_child：子进程处理函数，完成几个任务：重新注册epoll，信号；监听本进程上的事件表（即本进程所处理的socket以及主进程发来的信号）
 */
int run_child(int ipx, client_data *c_users, char *share_memory) {
    epoll_event events[MAX_EVENT_NUMBER];
    int child_epollfd = epoll_create(5);
    assert(child_epollfd != -1);
    int child_pipefd = c_users[ipx].pipefd[1];
    addfd(child_epollfd, child_pipefd);
    int child_connfd = c_users[ipx].connfd;
    addfd(child_epollfd, child_connfd);
    int ret;
    addsig(SIGTERM, child_term_handler, false);

    while (!stop_child) {
        int e_number = epoll_wait(child_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (e_number < 0 && errno != EINTR) {
            printf("epoll failed!\n");
            break;
        }

        for (int i = 0; i < e_number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == child_connfd && events[i].events & EPOLLIN) {
                memset(share_memory + ipx * BUFFER_SIZE, '\0', BUFFER_SIZE);
                ret = recv(child_connfd, share_memory + ipx * BUFFER_SIZE, BUFFER_SIZE - 1, 0);
                if (ret < 0 && errno != EAGAIN ) {
                    printf("exit 1\n");
                    stop_child = true;
                }
                else if (ret == 0) {
                    printf("exit 2\n");
                    stop_child = true;
                }
                else {
                    send(child_pipefd, (char*)&ipx, sizeof(ipx), 0);
                }
            }
            else if (sockfd == child_pipefd && events[i].events & EPOLLIN) {
                int client;
                ret = recv(child_pipefd, (char*)&client, sizeof(client), 0);
                if (ret < 0 && errno != EAGAIN) {
                    printf("exit 3\n");
                    stop_child = true;
                }
                else if (ret == 0) {
                    continue;
                }
                else {
                    send(child_connfd, share_memory+client*BUFFER_SIZE, BUFFER_SIZE, 0);
                }
            }
            else {
                continue;
            }
        }
    }
    close(child_connfd);
    close(child_pipefd);
    close(child_epollfd);
    printf("Client %d has quited\n", child_connfd);
    return 0;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
int main (int argc, char* argv[]) {

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

    listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );

    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );

    ret = listen( listenfd, 5 );
    assert( ret != -1 );

    // 初始化记录用户信息的数据结构
    user_count = 0;
    users = new client_data[USER_LIMIT];
    // 初始化记录子进程信息的数据结构
    sub_process = new int [PROCESS_LIMIT];
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        sub_process[i] = -1;
    }
    // 初始化epoll
    epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, listenfd);

    // 注册信号
    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, sig_handler);
    bool stop_server = false;
    bool terminate = false;

    /*
     * 创建共享内存
     */
    shmfd = shm_open(shm_name, O_CREAT|O_RDWR, 0666);
    assert(shmfd != -1);
    // 调整共享内存大小（ftruncate的作用是调节文件的大小，而共享内存实际上就是一个读到内存中的特殊文件）
    ret = ftruncate(shmfd, USER_LIMIT*BUFFER_SIZE);
    assert(ret != -1);
    share_mem = (char*)mmap(NULL, USER_LIMIT * BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    assert(share_mem != MAP_FAILED);
    close(shmfd); // ???

    while (!stop_server) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            printf("Server epoll failed\n");
            break;
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
                if (user_count >= USER_LIMIT) {
                    const char* info = "Too many Users\n";
                    printf("%s", info);
                    send(connfd, info, sizeof(info), 0);
                    close(connfd);
                    continue;
                }
                users[user_count].address = client_addr;
                users[user_count].connfd = connfd;
                ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_count].pipefd);
                assert(ret != -1);
                pid_t pid = fork();
                if (pid < 0) {
                    printf("Fork failed\n");
                    close(connfd);
                    continue;
                }
                else if (pid == 0) {
                    close(epollfd);
                    close(listenfd);
                    close(users[user_count].pipefd[0]);
                    close(sig_pipefd[0]);
                    close(sig_pipefd[1]);
                    run_child(user_count, users, share_mem);
                    munmap((void*)share_mem, USER_LIMIT*BUFFER_SIZE);
                    printf("A child has finished\n");
                    exit(0);
                }
                else {
                    close(connfd);
                    close(users[user_count].pipefd[1]);
                    addfd(epollfd, users[user_count].pipefd[0]);
                    users[user_count].pid = pid;
                    sub_process[pid] = user_count;
                    user_count++;
                }
            }
            else if (sockfd == sig_pipefd[0] && events[i].events & EPOLLIN) {
                int sig;
                char signals[1024];
                ret = recv(sockfd, signals, 1024, 0);
                if (ret == -1) {
                    continue;
                }
                else if (ret == 0) {
                    continue;
                }
                else {
                    for (int j = 0; j < ret; ++j) {
                        switch (signals[j]) {
                            case SIGCHLD: {
                                pid_t pid;
                                int stat;
                                while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
                                    int del_user = sub_process[pid];
                                    sub_process[pid] = -1;
                                    if (del_user < 0 || del_user > USER_LIMIT) {
                                        continue;
                                    }
                                    epoll_ctl(epollfd, EPOLL_CTL_DEL, users[del_user].pipefd[0], 0);
                                    close(users[del_user].connfd);
                                    users[del_user] = users[--user_count];
                                    // 因为user数组已经改变，所以需要调整sub_process与users的映射关系
                                    sub_process[users[del_user].pid] = del_user;
                                }
                                if (terminate && user_count == 0) {
                                    printf("stop server!\n");
                                    stop_server = true;
                                    break;
                                }
                                case SIGTERM:
                                case SIGINT: {
                                    printf("kill all the child now\n");
                                    if (user_count == 0) {
                                        stop_server = true;
                                        break;
                                    }
                                    for (int k = 0; k < user_count; ++k) {
                                        pid_t pid_k = users[k].pid;
                                        kill(pid_k, SIGTERM);
                                    }
                                    terminate = true;
                                    break;
                                }
                                default:
                                    break;
                            }
                        }
                    }
                }
            }
            else if (events[i].events & EPOLLIN) {
                int child = 0;
                ret = recv(sockfd, (char*) &child, sizeof(child), 0);
                if (ret < 0) {
                    continue;
                }
                else if (ret == 0) {
                    continue;
                }
                else {
                    for (int j = 0; j < user_count; ++j) {
                        if (users[j].pipefd[0] != sockfd) {
                            printf("send data to child process pipe!\n");
                            send(users[j].pipefd[0], (char*) &child, sizeof(child), 0);

                        }
                    }
                }
            }

        }
    }
    del_resource();
    return 0;
}
#pragma clang diagnostic pop
#endif //TINYWEBSERVER_MY_SHM_SERVER_H
