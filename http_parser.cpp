//
// Created by daqige on 2020/12/21.
//
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <string>
#include <iostream>

using namespace std;

#define BUFFER_SIZE 4096
enum CHECK_STATE {
    CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT
};
enum LINE_STATUS {
    LINE_OK = 0, LINE_BAD, LINE_OPEN
};
enum HTTP_CODE {
    NO_REQUEST, GET_REQUEST, BAD_REQUEST, FORBIDDEN_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION
};
static const char *szret[] = {"I get a correct result\n", "Something wrong\n"};

LINE_STATUS parse_line(char* buffer, int &check_index, int &read_index) {
    char temp;
    for (; check_index < read_index; check_index++) {
        temp = buffer[check_index];
        if (temp == '\r') {
            if (check_index + 1 == read_index) {
                return LINE_OPEN;
            }
            else if (buffer[check_index + 1] == '\n') {
                buffer[check_index++] = '\0';
                buffer[check_index++] = '\0';
                return LINE_OK;
            }
            else {
                return LINE_BAD;
            }
        }
        else if (temp == '\n') {
            if (check_index > 1 && buffer[check_index - 1] == '\r') {
                buffer[check_index - 1] = '\0';
                buffer[check_index++] = '\0';
                return LINE_OK;
            }
            else {
                return LINE_BAD;
            }
        }

//        return LINE_BAD; 原书位置错误
    }
    return LINE_OPEN;
}

HTTP_CODE parse_request_line(char* buffer, CHECK_STATE &checkState) {
    char* url = strpbrk(buffer, " \t");
    if (!url) {
        return BAD_REQUEST;
    }

    // * 与 ++优先级相等，所以是先对url解除引用赋值然后再将url指针自增
    *url++ = '\0';

    char* method = buffer;
    if (strcasecmp(method, "GET") == 0) {
        cout << "This request method is GET!\n";
    }
    else{
        return BAD_REQUEST;
    }
    url += strspn(url, " \t");
    char* version = strpbrk(url, " \t");
    if (!version) {
        return BAD_REQUEST;
    }
    *version++ = '\0';
    version += strspn(version, " \t");
    if (strcasecmp(version, "HTTP/1.0") != 0) {
        return BAD_REQUEST;
    }
    else{
        cout << "The HTTP Version: " << version << " is Supported!" << endl;
    }

    if (strncasecmp(url, "http://", 7) == 0) {
        url += 7;
        url = strchr(url, '/');
    }

    if (!url || url[0] != '/') {
        return BAD_REQUEST;
    }
    cout << "The Request URL Is " << url << endl;
    checkState = CHECK_STATE_HEADER;

    return NO_REQUEST;
}

HTTP_CODE parse_header(char* buffer, CHECK_STATE checkState) {
    if (buffer[0] == '\0') {
        return GET_REQUEST;
    }
    else if (strncasecmp(buffer, "HOST:", 5) == 0) {
        char* host = strpbrk(buffer, " \t");
        host += strspn(host, " \t");
        cout << "The Request Host is " << host << endl;
    }
    else if (strncasecmp(buffer, "Connection:", 11) == 0) {
        char* connection = strpbrk(buffer, " \t");
        connection += strspn(connection, " \t");
        cout << "The Request Connection is " << connection << endl;
    }
    else {
        char* temp = strpbrk(buffer, " \t");
        if (!temp) {
            return NO_REQUEST;
        }
        *temp++ = '\0';
        char* unknown = buffer;
        cout << "Can't handle the header: " << unknown << endl;
    }
    return NO_REQUEST;
}

HTTP_CODE parse_content(char* buffer, int &check_index, CHECK_STATE &checkState, int &read_index, int &start_line){
    LINE_STATUS lineStatus = LINE_OK;
    HTTP_CODE retCode = NO_REQUEST;
    while ((lineStatus = parse_line(buffer, check_index, read_index)) == LINE_OK) {
        char* temp = buffer + start_line;
        start_line = check_index;
        switch (checkState) {
            case CHECK_STATE_REQUESTLINE:
            {
                retCode = parse_request_line(temp, checkState);
                if (retCode == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }

            case CHECK_STATE_HEADER:
            {
                retCode = parse_header(temp, checkState);
                if (retCode == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                else if (retCode == GET_REQUEST){
                    return GET_REQUEST;
                }
                break;
            }

            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }

    if (lineStatus == LINE_OPEN) {
        return NO_REQUEST;
    }
        // lineStatus == LINE_BAD 状态下主状态机的反应
    else {
        return BAD_REQUEST;
    }
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
int main(int argv, char* argc[]) {
    if (argv <= 2) {
        cout << "The number of arguments are not matched!\n";
        return -1;
    }
    const char* ip = argc[1];
    int port = atoi(argc[2]);

    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_port = htons(port);
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);

    ret = listen(listenfd, 5);
    assert(ret >= 0);

    struct sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);
    int fd = accept(listenfd, (struct sockaddr*)&client_address, &client_len);
    if (fd < 0) {
        cout << "The error is " << errno << endl;
    }
    else {
        char buffer[BUFFER_SIZE];
        memset(buffer, '\0', BUFFER_SIZE);
        int read_index = 0;
        int check_index = 0;
        int data_read = 0;
        int start_line = 0;
        CHECK_STATE checkState = CHECK_STATE_REQUESTLINE;
        while (1) {
            data_read = recv(fd, buffer+read_index, BUFFER_SIZE-read_index, 0);
            if (data_read == -1) {
                cout << "Reading Failed\n";
                break;
            }
            else if (data_read == 0) {
                cout << "Remote Client has Closed Connection\n";
                break;
            }
            read_index += data_read;
            HTTP_CODE result = parse_content(buffer, check_index, checkState, read_index, start_line);
            if (result == GET_REQUEST) {
                send(fd, szret[0], sizeof(szret[0]), 0);
                break;
            }
            else if (result == NO_REQUEST) {
                continue;;
            }
            else {
                send(fd, szret[1], sizeof(szret[1]), 0);
                break;
            }
        }
    }
    close(fd);
    return 0;
}
#pragma clang diagnostic pop