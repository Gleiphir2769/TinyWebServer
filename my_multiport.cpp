//
// Created by daqige on 2020/12/24.
//
#include <sys/types.h>
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
#include <pthread.h>
#include <iostream>

#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

using namespace std;

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, new_option);
    return old_option;
}

void addfd(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
int main(int argc, char* argv[]) {
    if (argc <= 2) {
        cout << "The number of arguments are not matched!\n";
        return -1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_port = htons(port);
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    
    int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(tcpfd >= 0);
    
    int ret = bind(tcpfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    ret = listen(tcpfd, 5);
    assert(ret != -1);
    
    int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    assert(udpfd >= 0);
    ret = bind(udpfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);
    
    // events的作用是承接epoll_wait所传来的内核事件表，实际内核事件表的建议大小由epoll_creat指出
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd, tcpfd);
    addfd(epollfd, udpfd);
    
    while (1) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0) {
            cout << "Epoll failed\n";
            break;
        }

        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == tcpfd) {
                sockaddr_in client_address;
                socklen_t client_length = sizeof(client_address);
                int connfd = accept(sockfd, (struct sockaddr*)&client_address, &client_length);
                // 注册connfd到内核事件表
                addfd(epollfd, connfd);
            }
            else if (sockfd == udpfd) {
                char buf[UDP_BUFFER_SIZE];
                memset(buf, '\0', UDP_BUFFER_SIZE);
                struct sockaddr_in client_address;
                socklen_t client_length = sizeof(client_address);
                ret = recvfrom(udpfd, buf, UDP_BUFFER_SIZE-1, 0, (struct sockaddr*)&client_address, &client_length);
                if (ret > 0) {
                    sendto(udpfd, buf, UDP_BUFFER_SIZE-1, 0, (struct sockaddr*)&client_address, client_length);
                }
            }
            else if (events[i].events & EPOLLIN){
                char buf[TCP_BUFFER_SIZE];
                while(1) {
                    memset(buf, '\0', TCP_BUFFER_SIZE);
                    ret = recv(sockfd, buf, TCP_BUFFER_SIZE, 0);
                    if (ret < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        // 接受完一次数据就关闭该tcp链接
                        close(sockfd);
                        break;
                    }
                    else if (ret == 0) {
                        cout << "Something happened\n";
                        close(sockfd);
                    }
                    else {
                        send(sockfd, buf, TCP_BUFFER_SIZE, 0);
                    }
                }
            }
            else {
                cout << "Something  else happened\n";
            }
        }
    }
    close(tcpfd);
    return 0;
}
#pragma clang diagnostic pop
