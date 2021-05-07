//
// Created by daqige on 2021/1/10.
//

#ifndef TINYWEBSERVER_HTTP_CONN_H
#define TINYWEBSERVER_HTTP_CONN_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
#include "lock/locker.h"
#include "log/log.h"

class http_conn {
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH };
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };
public:
    http_conn() {};
    ~http_conn() {};

public:
    void init(int sockfd, const sockaddr_in address);
    void close_conn(bool real_close = true);
    void process();
    bool read();
    bool write();
private:
    // 初始化新连接
    void init();
    // 解析http请求
    HTTP_CODE process_read();
    // 填充http应答
    bool process_write(HTTP_CODE ret);

    /*
     * 用于被process_read调用的一组函数
     */
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_headers(char* text);
    HTTP_CODE parse_content(char* text);
    // 响应读取请求的文件
    HTTP_CODE do_request();
    char* get_line(){return m_read_buf + m_start_line;};
    // 从状态机
    LINE_STATUS parse_line();

    /*
     * 用于被process_write调用的一组函数
     */
    void unmap();
    bool add_response(const char* format, ...);
    bool add_headers(int content_len);
    bool add_content(const char* content);
    bool add_status_line(int status, const char * title);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
private:
    // 该http的地址和对方的地址
    int m_sockfd;
    sockaddr_in m_address;

    char m_read_buf[ READ_BUFFER_SIZE ];
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;
    char m_write_buf[ WRITE_BUFFER_SIZE ];
    int m_write_idx;

    CHECK_STATE m_check_state;
    METHOD m_method;

    char m_real_file[ FILENAME_LEN ];
    char* m_url;
    char* m_version;
    char* m_host;
    int m_content_length;
    bool m_linger;

    char* m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;

};

#endif //TINYWEBSERVER_HTTP_CONN_H
