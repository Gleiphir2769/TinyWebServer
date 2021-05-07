//
// Created by daqige on 2021/2/28.
//

#ifndef TINYWEBSERVER_SQL_CONNECTION_POOL_H
#define TINYWEBSERVER_SQL_CONNECTION_POOL_H

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "../lock/locker.h"

using namespace std;

class sql_connection_pool {
public:
    MYSQL* GetConnection();
    bool ReleaseConnection(MYSQL *con);
    unsigned int GetFreeConnectionNum();
    void DestroyPool();

    // 单例模式
    static sql_connection_pool* GetInstance();

    void init(string url, string User, string PassWord, string DataBaseName, int Port, unsigned int MaxConn);

    sql_connection_pool();
    ~sql_connection_pool();

private:
    Locker lock;
    std::list<MYSQL *>connList;
    Sem reserve;

private:
    string url;			 //主机地址
    int Port;		 //数据库端口号
    string User;		 //登陆数据库用户名
    string PassWord;	 //登陆数据库密码
    string DatabaseName; //使用数据库名

private:
    unsigned int MaxConn;  //最大连接数
    unsigned int CurConn;  //当前已使用的连接数
    unsigned int FreeConn; //当前空闲的连接数
};

class connectionRAII{
public:
    connectionRAII(MYSQL **con, sql_connection_pool *connPool);
    ~connectionRAII();

private:
    MYSQL* connRAII;
    sql_connection_pool* poolRAII;
};


#endif //TINYWEBSERVER_SQL_CONNECTION_POOL_H
