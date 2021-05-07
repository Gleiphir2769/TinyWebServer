cmake_minimum_required(VERSION 3.16)
project(TinyWebServer)

set(CMAKE_CXX_STANDARD 14)

# 先设置路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

add_executable(TinyWebServer http_server/http_conn.cpp http_server/http_conn.h http_server/log/block_queue.h http_server/log/log.cpp http_server/log/log.h http_server/lock/locker.h threadpool.h http_server/CGImysql/sql_connection_pool.h http_server/CGImysql/sql_connection_pool.cpp http_server/CGImysql/main.cpp)