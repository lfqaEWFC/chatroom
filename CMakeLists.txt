cmake_minimum_required(VERSION 3.10)

project(chatroom)

set(MYSQL_INCLUDE_DIR /usr/include/mysql)
set(MYSQL_LIBRARIES /usr/lib/x86_64-linux-gnu/libmysqlclient.so)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/define
    ${PROJECT_SOURCE_DIR}/Database
    ${PROJECT_SOURCE_DIR}/cli
    ${PROJECT_SOURCE_DIR}/ser
    ${MYSQL_INCLUDE_DIR}
)

add_executable(client
    cli/chat_client.cpp
    cli/menu.cpp
    include/inetsockets_tcp.cpp
    include/Threadpool.cpp
    cli/User.cpp
)

target_link_libraries(client
    hiredis
    ncurses
    readline
)

add_executable(server
    ser/serve.cpp
    ser/chat_serve.cpp
    include/inetsockets_tcp.cpp
    include/Threadpool.cpp
    Database/Database.cpp
)

target_link_libraries(server
    hiredis
    ncurses
    readline
    ${MYSQL_LIBRARIES}
)

# FTP_serve
add_executable(FTP_serve
    ser/FTP_serve.cpp
    include/inetsockets_tcp.cpp
    include/Threadpool.cpp
)

target_link_libraries(FTP_serve
    hiredis
    ncurses
    readline
)
