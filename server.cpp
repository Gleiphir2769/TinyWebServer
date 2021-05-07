//
// Created by daqige on 2020/12/23.
//
#define _GNU_SOURCE 1
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
#include <poll.h>
#include <iostream>

#define USER_LIMIT 5
#define BUFFER_SIZE 64
#define FD_LIMIT 65535

using namespace std;

struct client_data {
    sockaddr_in address;
    char* write_buf;
    char buf[BUFFER_SIZE];
};

int setnonblocking(int sockfd) {
    int old_option = fcntl(sockfd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(sockfd, F_SETFL, new_option);
    return old_option;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
int main(int argv, char* argc[]) {
    if (argv <= 2) {
        cout << "The arguments are not enough\n";
        return 1;
    }
    const char *ip = argc[1];
    int port = atoi(argc[2]);

    struct sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_address.sin_addr);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd>=0);
    int ret = bind(listenfd, (struct sockaddr*)&server_address, sizeof(server_address));
    assert(ret != -1);
    ret = listen(listenfd, 5);
    assert(ret != -1);

    // Users数组，使用客户端的sockfd当作数组索引，是一种空间换时间的方式
    client_data* users = new client_data[FD_LIMIT];

    pollfd fds[USER_LIMIT+1];
    int user_counter = 0;
    // 初始化poll
    for (int i = 1; i <= USER_LIMIT; ++i) {
        fds[i].fd = -1;
        fds[i].events = 0;
    }

    fds[0].fd = listenfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;


    while (1) {
        ret = poll(fds, user_counter+1, -1);
        if (ret < 0) {
            cout << "Poll Failed!\n";
            break;
        }

        for (int i = 0; i < user_counter+1; ++i) {
            if ((fds[i].fd == listenfd) && (fds[i].revents & POLLIN)) {
                struct sockaddr_in client_address;
                socklen_t clientlen = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &clientlen);
                if (connfd < 0) {
                    cout << "error is " << errno << endl;
                    continue;
                }
                if (user_counter >= USER_LIMIT) {
                    const char* info = "Too much users!\n";
                    cout << info;
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }
                user_counter++;
                // 在用户表中注册用户
                users[connfd].address = client_address;
                // 设置用户socket为非阻塞
                setnonblocking(connfd);
                // 在事件表中注册该sockid
                fds[user_counter].fd = connfd;
                fds[user_counter].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_counter].revents = 0;
                cout << "Comes a new user, now have " << user_counter << " users!\n";
            }
            else if (fds[i].revents & POLLERR) {
                cout << "Poll failed from " << fds[i].fd << endl;
                // errors数组仅用于承接getsockopt所写入的数据，并不关心实际数据是什么
                // 同样，getsockopt在这里的功能只是清除掉socket的错误状态，并不关心实际是什么引起了错误
                char errors[100];
                memset(errors, '\0', 100);
                socklen_t length = sizeof(errors);
                if (getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0) {
                    cout << "Get socket option failed!\n";
                }
                continue;
            }
            else if (fds[i].revents & POLLRDHUP) {
                int connfd = fds[i].fd;
                close(connfd);
                users[connfd] = users[fds[user_counter].fd];
                fds[i] = fds[user_counter];
                user_counter--;
                i--;
                cout << "a client left\n";
            }
            else if (fds[i].revents & POLLIN) {
                int connfd = fds[i].fd;
                memset(users[connfd].buf, '\0', BUFFER_SIZE);
                ret = recv(connfd, users[connfd].buf, BUFFER_SIZE-1, 0);
                if (ret < 0)
                {
                    if (errno != EAGAIN) {
                        close(connfd);
                        users[connfd] = users[fds[user_counter].fd];
                        fds[i] = fds[user_counter];
                        user_counter--;
                        i--;
                        cout << "Read a client failed\n";
                    }
                }
                else if (ret == 0) {
                    cout << "Received no data\n";
                    continue;
                }
                else {
                    cout << "Get " << ret << " bytes data: " << users[connfd].buf << " from client " << connfd << endl;
                    for (int j = 1; j <= user_counter; ++j) {
                        if (fds[j].fd != connfd) {
                            fds[j].events |= ~POLLIN;
                            fds[j].events |= POLLOUT;
                            users[fds[j].fd].write_buf = users[connfd].buf;
                        }
                    }
                }
            }
            else if (fds[i].revents & POLLOUT) {
                int connfd = fds[i].fd;
                // todo: 注意！！！如果不检查write_buf，对一个NULL当作缓冲区读数据会直接引发内核错误
                if( ! users[connfd].write_buf )
                {
                    continue;
                }
                ret = send(connfd, users[connfd].write_buf, strlen(users[connfd].write_buf), 0);
                users[connfd].write_buf = NULL;
                fds[i].events |= ~POLLOUT;
                fds[i].events |= POLLIN;
            }

        }
    }
    delete [] users;
    close(listenfd);
    return 0;
}
#pragma clang diagnostic pop