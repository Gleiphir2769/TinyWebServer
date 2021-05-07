//
// Created by daqige on 2020/12/25.
//

#ifndef TINYWEBSERVER_TIME_H
#define TINYWEBSERVER_TIME_H

#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define BUFFER_SIZE 1024

class util_timer;

struct client_data {
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer* timer;
};

class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {};

private:
    time_t expire;
    void (*cb_func) (client_data* );
    client_data* user_data;
    util_timer* prev;
    util_timer* next;
};

class sort_timer_lst
{
public:
    sort_timer_lst() : head(NULL), tail(NULL) {};
    ~sort_timer_lst();



private:
    util_timer* head;
    util_timer* tail;
};


#endif //TINYWEBSERVER_TIME_H
