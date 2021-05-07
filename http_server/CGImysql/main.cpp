//
// Created by daqige on 2021/2/28.
//
#include "sql_connection_pool.h"
#include <map>
#include <iostream>


int main() {
    map<string, string> users;

    sql_connection_pool::GetInstance()->init("localhost", "root", "root", "my_innodb", 3306, 8);
    sql_connection_pool *connPool = sql_connection_pool::GetInstance();

    string query_string = "select name, passwd from user;";
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        printf("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        cout << row[0] << "\t" << row[1] << endl;
    }
/*    map<string, string>::reverse_iterator iter;
    for(iter = users.rbegin(); iter != users.rend(); iter++)

        cout<<iter->first<<"  "<<iter->second<<endl;*/
}