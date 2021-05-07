//
// Created by daqige on 2021/1/6.
//

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
#include "process_poll.h"


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

//static const char *shm_name = "/my_shm";
//int sig_pipefd[2];
//int epollfd;
//int listenfd;
//int shmfd;
//char *share_mem = 0;
//client_data *users = 0;
//int *sub_process = 0;
//int user_count = 0;
//bool stop_child = false;


//void del_resource() {
//    close(sig_pipefd[0]);
//    close(sig_pipefd[1]);
//    close(listenfd);
//    close(epollfd);
//    shm_unlink(shm_name);
//    delete[] users;
//    delete[] sub_process;
//}

/*
 * 形参sig的作用是与信号注册的信号处理函数形式一致，实际上本代码中信号处理函数有两个，仔细思考回调函数的意义
 */
//void child_term_handler(int sig) {
//    stop_child = true;
//}

class shm_server {
public:
    shm_server() {};
    ~shm_server() {};
    void process() {
        printf("Server %d start working!\n", id);
        sleep(2);
        printf("Server %d finished\n", id);
    }
    void init(int epollfd, int connfd, int pid, struct sockaddr_in addr) {
        printf("Process server %d initialized!\n", pid);
        id = pid;
    }

private:
    int id;
};

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

    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );

    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );

    ret = listen( listenfd, 5 );
    assert( ret != -1 );

    ProcessPool<shm_server>* poll = ProcessPool<shm_server>::create(listenfd);
    if (poll) {
        poll->run();
        delete poll;
    }

    close(listenfd);
//    del_resource();
    return 0;
}
#pragma clang diagnostic pop
#endif //TINYWEBSERVER_MY_SHM_SERVER_H
