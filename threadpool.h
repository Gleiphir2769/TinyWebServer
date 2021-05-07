//
// Created by daqige on 2021/1/8.
//

#ifndef TINYWEBSERVER_THREADPOOL_H
#define TINYWEBSERVER_THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

template<typename T>
class threadpool {
public:
    threadpool(int m_thread_number = 8, int m_max_request = 10000);

    ~threadpool();

    bool append(T *request);

private:
    static void *worker(void *arg);

    void run();

private:
    int m_thread_number;
    int m_max_request;
    pthread_t *m_threads;
    std::list<T *> m_workqueue;
    Locker m_queuelocker;
    Sem m_queuestat;
    bool m_stop;

};

template<typename T>
threadpool<T>::threadpool(int m_thread_number, int m_max_request) : m_thread_number(m_thread_number),
                                                                    m_max_request(m_max_request), m_stop(false),
                                                                    m_threads(NULL) {
    if (m_thread_number <= 0 || m_max_request <= 0) {
        throw std::exception();
    }

    m_threads = new pthread_t[m_max_request];
    if (!m_threads) {
        throw std::exception();
    }
    for (int i = 0; i < m_thread_number; ++i) {
        printf("Create the %dth thread\n", i);
        if (pthread_create(m_threads+i, NULL, worker, this) != 0) {
            delete [] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]) != 0) {
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool() {
    delete [] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T *request) {
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_request) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    // 添加一个任务便向信号量做一次v操作
    m_queuestat.post();
    return true;
}

template<typename T>
void* threadpool<T>::worker(void *arg) {
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run() {
    while (! m_stop) {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request) {
            continue;
        }
        request->process();
    }
}

#endif //TINYWEBSERVER_THREADPOOL_H
