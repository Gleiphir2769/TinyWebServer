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
#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>
#include <iostream>

#define BUFFER_SIZE 64
using namespace std;
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"

int main(int argv, char *argc[]) {
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

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (connect(sockfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        cout << "Connection Failed\n";
        close(sockfd);
        return 1;
    }

    pollfd fds[2];
    // 标准输入
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].revents = 0;

    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret = pipe(pipefd);
    assert(ret != -1);

    while (1) {
        ret = poll(fds, 2, -1);
        if (ret < 0) {
            cout << "Poll Failed\n";
            break;
        }
        else if (fds[1].revents & POLLRDHUP) {
            cout << "Connection closed\n";
            break;
        }
        else if (fds[1].revents & POLLIN) {
            memset(read_buf, '\0', BUFFER_SIZE);
            ret = recv(fds[1].fd, read_buf, BUFFER_SIZE - 1, 0);
            cout << "User: " << fds[1].fd << " said: " << read_buf << endl;
        }
        if (fds[0].revents & POLLIN) {
            ret = splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE);
            ret = splice(pipefd[0], NULL, sockfd, NULL, 32768, SPLICE_F_MORE);
        }
    }
    close(sockfd);
    return 0;
}

#pragma clang diagnostic pop
