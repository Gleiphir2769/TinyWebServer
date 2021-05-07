//
// Created by daqige on 2021/2/28.
//

#include "sql_connection_pool.h"


MYSQL *sql_connection_pool::GetConnection() {
    MYSQL* conn = nullptr;
    if (connList.empty())
        return nullptr;
    reserve.wait();
    lock.lock();

    conn = connList.front();
    connList.pop_front();

    --FreeConn;
    ++CurConn;

    return conn;
}

sql_connection_pool::sql_connection_pool() {
    this->CurConn = 0;
    this->MaxConn = 0;
    this->FreeConn = 0;
}

sql_connection_pool::~sql_connection_pool() {
    DestroyPool();
}

sql_connection_pool *sql_connection_pool::GetInstance() {
    static sql_connection_pool connPool;
    return &connPool;
}

void sql_connection_pool::init(string url, string User, string PassWord, string DataBaseName, int Port,
                               unsigned int MaxConn) {
    this->url = std::move(url);
    this->User = std::move(User);
    this->PassWord = std::move(PassWord);
    this->DatabaseName = std::move(DataBaseName);
    this->Port = Port;
    this->MaxConn = MaxConn;

    lock.lock();
    for (int i = 0; i < MaxConn; ++i) {
        MYSQL* conn = nullptr;
        conn = mysql_init(conn);
        if (conn == nullptr)
        {
            cout << "Error:" << mysql_error(conn);
            exit(1);
        }
        conn = mysql_real_connect(conn, url.c_str(), User.c_str(), PassWord.c_str(), DataBaseName.c_str(),
        Port, NULL, 0);

        if (conn == nullptr)
        {
            cout << "Error:" << mysql_error(conn);
            exit(1);
        }

        connList.push_back(conn);
        ++FreeConn;
    }
}

bool sql_connection_pool::ReleaseConnection(MYSQL *con) {
    if (NULL == con)
        return false;

    lock.lock();

    connList.push_back(con);
    ++FreeConn;
    --CurConn;

    lock.unlock();

    reserve.post();
    return true;
}

//销毁数据库连接池
void sql_connection_pool::DestroyPool()
{

    lock.lock();
    if (connList.size() > 0)
    {
        list<MYSQL *>::iterator it;
        for (it = connList.begin(); it != connList.end(); ++it)
        {
            MYSQL *con = *it;
            mysql_close(con);
        }
        CurConn = 0;
        FreeConn = 0;
        connList.clear();

        lock.unlock();
    }

    lock.unlock();
}

//当前空闲的连接数
unsigned int sql_connection_pool::GetFreeConnectionNum() {
    return this->FreeConn;
}


connectionRAII::connectionRAII(MYSQL **con, sql_connection_pool *connPool) {
    *con = connPool->GetConnection();
    connRAII = *con;
    poolRAII = connPool;
}

connectionRAII::~connectionRAII() {
    poolRAII->ReleaseConnection(connRAII);
}
