//
// Created by daqige on 2021/1/7.
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

#include "process_poll.h"

class cgi_conn {
public:
    cgi_conn() {};

    ~cgi_conn() {};

    void init(int epollfd, int sockfd, const sockaddr_in client_addr) {
        m_epollfd = epollfd;
        m_sockfd = sockfd;
        m_address = client_addr;
        memset(m_buf, '\0', BUFFER_SIZE);
        m_read_idx = 0;
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"

    void process() {
        int idx;
        int ret = -1;
        while (true) {
            idx = m_read_idx;
            ret = recv(m_sockfd, m_buf + idx, BUFFER_SIZE - idx - 1, 0);
            if (ret < 0) {
                if (errno != EAGAIN) {
                    removefd(m_epollfd, m_sockfd);
                }
                break;
            }
            else if (ret == 0) {
                removefd(m_epollfd, m_sockfd);
                break;
            }
            else {
                m_read_idx += ret;
                printf("User content is %s\n", m_buf);
                for (; idx < m_read_idx; ++idx) {
                    if (idx >= 1 && m_buf[idx] == '\n' && m_buf[idx-1] == '\r') {
                        break;
                    }
                }
                if (idx == m_read_idx) {
                    continue;
                }
                // 截断m_buf
                m_buf[idx-1] = '\0';

                char* file_name = m_buf;
                if (access(file_name, F_OK) == -1) {
                    printf("Invalid file name!\n");
                    removefd(m_epollfd, m_sockfd);
                    break;
                }
                ret = fork();
                if (ret == -1) {
                    printf("Fork failed!\n");
                    removefd(m_epollfd, m_sockfd);
                    break;
                }
                else if (ret > 0) {
                    removefd(m_epollfd, m_sockfd);
                    break;
                }
                else {
                    close(STDOUT_FILENO);
                    dup(m_sockfd);
                    execl(m_buf, m_buf, 0);
                    exit(0);
                }
            }
        }
    }

#pragma clang diagnostic pop
private:
    static const int BUFFER_SIZE = 1024;
    static int m_epollfd;
    int m_sockfd;
    sockaddr_in m_address;
    char m_buf[BUFFER_SIZE];
    int m_read_idx;
};

int cgi_conn::m_epollfd = -1;

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        printf("Not enough arguments\n");
        return 1;
    }

    char* ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd != -1);

    ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 5);
    assert(ret != -1);

    ProcessPool<cgi_conn>* pool = ProcessPool<cgi_conn>::create(listenfd);
    if (pool) {
        pool->run();
        delete pool;
    }
    close(listenfd);
    return 0;
}