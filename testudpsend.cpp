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

#define BUFFER_SIZE 1024

using namespace std;

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

    int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    assert(udpfd >= 0);

    const char *info = "This is a UDP message!\n";
    int ret = sendto(udpfd, info, strlen(info), 0, (struct sockaddr*)&address, sizeof(address));
    if (ret < 0) {
        cout << "Send Failed!\n";
        return -1;
    }
    else {
        cout << "Successful send " << ret << " bytes " << "data!\n";

    }
    while (1) {
        char buf[BUFFER_SIZE];
        memset(buf, '\0', BUFFER_SIZE);
        struct sockaddr_in server_address;
        socklen_t server_len = sizeof(server_address);
        ret = recvfrom(udpfd, buf, BUFFER_SIZE, 0, (struct sockaddr*)&server_address, &server_len);
        if (ret > 0) {
            cout << "Successful received " << ret << " bytes data: " << buf << endl;
            break;
        }
        else {
            cout << "Receive Failed\n";
            break;
        }
    }
    return 0;
}
#pragma clang diagnostic pop

