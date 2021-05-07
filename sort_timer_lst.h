//
// Created by daqige on 2020/12/25.
//

#ifndef TINYWEBSERVER_TIMER_LST_H
#define TINYWEBSERVER_TIMER_LST_H


#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
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

public:
    time_t expire;
    void (*cb_func) (client_data* );
    client_data* user_data;
    util_timer* prev;
    util_timer* next;
};

class sort_timer_lst {
public:
    sort_timer_lst() : head(NULL), tail(NULL) {};
    ~sort_timer_lst();
    void add_timer(util_timer* timer);
    void adjust_timer(util_timer* timer);
    void del_timer(util_timer* timer);
    void tick();

private:
    util_timer *head;
    util_timer *tail;
    // 在定时器链表给定头结点的部分链表情况下插入一个定时器
    void add_timer(util_timer* timer, util_timer* lst_head);
};


#endif //TINYWEBSERVER_TIMER_LST_H
