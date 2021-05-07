//
// Created by daqige on 2021/1/2.
//

#ifndef TINYWEBSERVER_TIMER_WHEEL_H
#define TINYWEBSERVER_TIMER_WHEEL_H

#include <time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <iostream>

using namespace std;

const static int BUFFER_SIZE = 64;

class tw_timer;

struct client_data {
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    tw_timer *timer;
};

class tw_timer {
public:
    tw_timer(int rot, int ts) : next(NULL), prev(NULL), user_data(NULL), rotation(rot), time_slot(ts) {};

public:
    int rotation;
    int time_slot;

    void (*cb_func)(client_data*);

    client_data *user_data;
    tw_timer *next;
    tw_timer *prev;
};

class time_wheel {
public:
    time_wheel() : cur_slot(0) {
        for (int i = 0; i < N; ++i) {
            slots[i] = NULL;
        }
    };

    ~time_wheel() {
        for (int i = 0; i < N; ++i) {
            tw_timer *tmp = slots[i];
            while (tmp) {
                slots[i] = tmp->next;
                delete tmp;
                tmp = slots[i];
            }
        }
    }

    tw_timer *add_timer(int timeout) {
        if (timeout < 0) {
            return NULL;
        }
        int ticks = 0;
        if (timeout < SI) {
            ticks = 1;
        }
        else {
            ticks = timeout / SI;
        }
        int rotation = ticks / N;
        int ts = (cur_slot + (ticks % N)) % N;
        tw_timer *timer = new tw_timer(rotation, ts);
        if (!slots[ts]) {
            cout << "Add timer, rotation is " << rotation << " , ts is " << ts << ", cur_slot is " << cur_slot << endl;
            slots[ts] = timer;
        }
        else {
            timer->next = slots[ts];
            slots[ts]->prev = timer;
            slots[ts] = timer;
        }
        return timer;
    }

    void del_timer(tw_timer *timer) {
        if (!timer) {
            return;
        }
        int ts = timer->time_slot;
        if (timer == slots[ts]) {
            slots[ts] = timer->next;
            if (slots[ts]) {
                slots[ts]->prev = NULL;
            }
            delete timer;
        }
        else {
            timer->prev->next = timer->next;
            if (timer->next) {
                timer->next->prev = timer->prev;
            }
            delete timer;
        }
    }

    void tick() {
        tw_timer* tmp = slots[cur_slot];
        cout << "current slot is " << cur_slot << endl;
        while (tmp) {
            cout << "Tick the timer once\n";
            if (tmp->rotation > 0) {
                tmp->rotation--;
                tmp = tmp->next;
            }
            else {
                tmp->cb_func(tmp->user_data);
                if (tmp == slots[cur_slot]) {
                    cout << "The header timer has time out\n";
                    slots[cur_slot] = tmp->next;
                    if (slots[cur_slot]) {
                        slots[cur_slot]->prev = NULL;
                    }
                    delete tmp;
                    tmp = slots[cur_slot];
                }
                else {
                    if (tmp->next) {
                        tmp->next->prev = tmp->prev;
                    }
                    tmp->prev->next = tmp->next;
                    tw_timer* tmp2 = tmp->next;
                    delete tmp;
                    tmp = tmp2;
                }
            }
        }
        cur_slot = ++cur_slot % N;
    }

private:
    // 时间轮上的槽
    static const int N = 60;
    // 槽间隔
    static const int SI = 1;
    tw_timer *slots[N];
    int cur_slot;
};

#endif //TINYWEBSERVER_TIMER_WHEEL_H
