//
// Created by daqige on 2021/1/6.
//

#ifndef TINYWEBSERVER_PROCESS_POLL_H
#define TINYWEBSERVER_PROCESS_POLL_H

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
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <iostream>

// 进程信息类
class process {
public:
    process() : m_pid(-1) {};
public:
    pid_t m_pid;
    int m_pipefd[2];
};

/*
 * 要求模板类定义init和process函数
 */
template<typename T>
class ProcessPool {
private:
    ProcessPool(int listenfd, int process_number = 8);

public:
    static ProcessPool<T> *create(int listenfd, int process_number = 8) {
        if (!m_instance) {
            m_instance = new ProcessPool<T>(listenfd, process_number);
        }
        return m_instance;
    }

    ~ProcessPool() {
        delete[] m_sub_process;
    }

    // 启动函数
    void run();

private:
    void setup_sig_pipe();

    void run_parent();

    void run_child();

private:
    // 总进程数限制
    const static int MAX_PROCESS_NUMBER = 16;
    // 用户空间进程数限制
    const static int USER_PER_PROCESS = 150000;
    // 总事件数限制
    const static int MAX_EVENT_NUMBER = 10000;
    // 进程中的进程总数
    int m_process_number;
    // 子进程在池中的序号
    int m_idx;
    // 每个进程的内核事件表
    int m_epollfd;
    // 监听socket
    int m_listenfd;
    // 线程终止标志
    int m_stop;
    // 进程池所持有的的所有进程（指向一个进程数组的指针）
    process *m_sub_process;
    // 进程池静态实例
    static ProcessPool<T> *m_instance;

};
// 一般来说，m_instance是到构造函数中再进行初始化，但是进程池类要实现单体模式的创建，所以使用了这种特殊的初始化方式
template<typename T>
ProcessPool<T>* ProcessPool<T>::m_instance = nullptr;

static int sig_pipefd[2];

static int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

static void addfd(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

static void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

static void sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char *) &msg, 1, 0);
    errno = save_errno;
}

static void addsig(int sig, void( handler )(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

template<typename T>
ProcessPool<T>::ProcessPool(int listenfd, int process_number) : m_listenfd(listenfd), m_process_number(process_number),
                                                                m_idx(-1), m_stop(false) {
    assert(process_number > 0 && process_number <= MAX_PROCESS_NUMBER);
    m_sub_process = new process[MAX_PROCESS_NUMBER];
    assert(m_sub_process);

    for (int i = 0; i < MAX_PROCESS_NUMBER; ++i) {
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
        assert(ret == 0);

        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid >= 0);

        if (m_sub_process[i].m_pid > 0) {
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }
        else {
            // 子进程终止循环，因为只有父进程循环初始化
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            break;
        }
    }
}

// 统一事件源
template<typename T>
void ProcessPool<T>::setup_sig_pipe() {
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);

    setnonblocking(sig_pipefd[1]);
    addfd(m_epollfd, sig_pipefd[0]);

    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, sig_handler);
}

template<typename T>
void ProcessPool<T>::run() {
    if (m_idx != -1) {
        run_child();
    }
    run_parent();
}

// 子进程运行函数
template<typename T>
void ProcessPool<T>::run_child() {
    setup_sig_pipe();

    // 用于与主进程通信的管道
    int pipefd = m_sub_process[m_idx].m_pipefd[1];
    addfd(m_epollfd, pipefd);

    epoll_event events[MAX_EVENT_NUMBER];
    T *users = new T[USER_PER_PROCESS];
    assert(users);
    int number;
    int ret;

    while (!m_stop) {
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            printf("Epoll failed!\n");
            break;
        }

        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == pipefd && events[i].events & EPOLLIN) {
                int client = 0;
                ret = recv(sockfd, (char*)&client, sizeof(client), 0);
                if (ret < 0 && errno != EAGAIN || ret == 0) {
                    continue;
                }
                else {
                    struct sockaddr_in client_addr;
                    socklen_t client_addrlen = sizeof(client_addr);
                    int connfd = accept(m_listenfd, (struct sockaddr*)&client_addr, &client_addrlen);
                    if (connfd < 0 ) {
                        printf("Connect failed!\n");
                        continue;
                    }
                    addfd(m_epollfd, connfd);
                    // 模板类必须定义此方法
                    users[connfd].init(m_epollfd, connfd, client_addr);
                }
            }
            else if (sockfd == sig_pipefd[0] && events[i].events & EPOLLIN) {
                int sig;
                char signals[1024];
                ret = recv(sockfd, signals, sizeof(signals), 0);
                if (ret <= 0) {
                    continue;
                }
                else {
                    for (int j = 0; j < ret; ++j) {
                        switch (signals[j]) {
                            case SIGCHLD: {
                                pid_t pid;
                                int stat;
                                while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
                                    continue;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT: {
                                m_stop = true;
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }
            }
            else if (events[i].events & EPOLLIN) {
                // 模板类必须定义此方法
                users[sockfd].process();
            }
            else {
                continue;
            }
        }
    }
    delete []users;
    users = NULL;
    // 关闭与主进程通信的管道
    close(pipefd);
}

// 父进程执行函数
template <typename T>
void ProcessPool<T>::run_parent() {
    setup_sig_pipe();

    addfd(m_epollfd, m_listenfd);

    epoll_event events [MAX_EVENT_NUMBER];
    int sub_progress_counter = 0;
    int new_conn = 0;

    while (!m_stop) {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, 0);
        if (number < 0 && errno != EINTR) {
            printf("Epoll failed!\n");
            continue;
        }
        for (int i = 0; i < number; ++i) {
             int sockfd = events[i].data.fd;
             if (sockfd == m_listenfd) {
                 int rc = sub_progress_counter;
                 do {
                     if (m_sub_process[rc].m_pid != -1) {
                         break;
                     }
                     rc = (rc+1)%m_process_number;
                 }
                 while (rc != sub_progress_counter);
                 if (m_sub_process[rc].m_pid == -1) {
                     printf("There are no valid progress to distribute!\n");
                     m_stop = true;
                     break;
                 }
                 sub_progress_counter = (rc+1)%m_process_number;
                 new_conn = m_sub_process[rc].m_pid;
                 send(m_sub_process[rc].m_pipefd[0], (char*)&new_conn, sizeof(new_conn), 0);
                 printf("A new request has come and send it to child!\n");
             }
             else if (sockfd == sig_pipefd[0] && events[i].events & EPOLLIN) {
                 int sig;
                 char signals[1024];
                 int ret = recv(sockfd, signals, sizeof(signals), 0);
                 if (ret <= 0) {
                     continue;
                 }
                 else {
                     for (int j = 0; j < ret; ++j) {
                         sig = signals[j];
                         switch (sig) {
                             case SIGCHLD: {
                                 pid_t pid;
                                 int stat;
                                 while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
                                     for (int k = 0; k < m_process_number; ++k) {
                                         if (m_sub_process[k].m_pid == pid) {
                                             printf("child %d join\n", k);
                                             close(m_sub_process[k].m_pipefd[1]);
                                             m_sub_process[k].m_pid = -1;
                                         }
                                     }
                                 }
                                 // 检查若所有子进程退出后，父进程也推出
                                 m_stop = true;
                                 for (int k = 0; k < m_process_number; ++k) {
                                     if (m_sub_process[k].m_pid != -1) {
                                         m_stop = false;
                                     }
                                 }
                                 break;
                             }
                             case SIGTERM:
                             case SIGINT: {
                                 printf("Kill all the child progress\n");
                                 for (int k = 0; k < m_process_number; ++k) {
                                     int pid = m_sub_process[k].m_pid;
                                     if (pid != -1) {
                                         kill(pid, SIGTERM);
                                     }
                                 }
                                 break;
                             }
                             default:
                                 break;
                         }
                     }
                 }
             }
             else {
                 continue;
             }
        }
    }
    close(m_epollfd);
}


#endif //TINYWEBSERVER_PROCESS_POLL_H
