//
// Created by daqige on 2020/12/25.
//

#include "sort_timer_lst.h"

sort_timer_lst::~sort_timer_lst() {
    util_timer *tmp = head;

}

void sort_timer_lst::add_timer(util_timer *timer, util_timer *lst_head) {
    util_timer *prev = lst_head;
    util_timer *temp = lst_head->next;

    while (temp) {
        if (timer->expire < temp->expire) {
            prev->next = timer;
            timer->next = temp;
            timer->prev = prev;
            temp->prev = timer;
            break;
        }
        temp = temp->next;
        prev = prev->next;
    }
}

void sort_timer_lst::add_timer(util_timer *timer) {
    if (!timer) {
        return;
    }
    if (!head) {
        head = tail = timer;
        return;
    }
    if (timer->expire < head->expire) {
        util_timer *tmp = head;
        head = timer;
        timer->next = tmp;
        tmp->prev = timer;
        return;
    }
    // 两个重载的函数互相配合完成插入，因为链表没有设置头节点，所以公有函数检查头节点是否需要
    // 替换，私有函数完成实际插入操作；
    add_timer(timer, head);
}

void sort_timer_lst::adjust_timer(util_timer *timer) {
    if (!timer) {
        return;
    }
    if (!head) {
        head = tail = timer;
        return;
    }
    /* 从代码复用和效率的角度上，将节点取出再插入是最有效的方法，因为用循环扫描链表插入
     * 位置每次都要从头开始遍历，而实际上链表是一个除被调整节点外的有序链表*/
    util_timer *tmp = timer->next;
    if (!tmp || timer->expire < tmp->expire) {
        return;
    }
    if (timer == head) {
        head = timer->next;
        head->prev = nullptr;
        timer->next = nullptr;
        add_timer(timer, head);
    }
    else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void sort_timer_lst::del_timer(util_timer *timer) {
    if (!timer) {
        return;
    }

    if (timer == head && timer == tail) {
        delete timer;
        head = nullptr;
        tail = nullptr;
        return;
    }
    else if (timer == head) {
        head = head->next;
        head->prev = nullptr;
        delete timer;
        return;
    }
    else if (timer == tail) {
        timer->prev->next= nullptr;
        delete timer;
        return;
    }
    else {
        timer->next->prev = timer->prev;
        timer->prev->next = timer->next;
        delete timer;
        return;
    }
}

void sort_timer_lst::tick() {
    if (!head) {
        return;
    }

    std::cout << "Timer tick!\n";
    time_t cur = time(NULL);
    util_timer* tmp = head;
    while (tmp) {
        if (cur < tmp->expire) {
            break;
        }
        // 超时调用回调函数
        tmp->cb_func(tmp->user_data);
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}
















