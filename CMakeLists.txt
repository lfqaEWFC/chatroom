cmake_minimum_required (VERSION 3.10)

project (chatroom)

set(MYSQL_INCLUDE_DIR /usr/include/mysql)
set(MYSQL_LIBRARIES /usr/lib/x86_64-linux-gnu/libmysqlclient.so)

add_executable(client
    ${PROJECT_SOURCE_DIR}/cli/chat_client.cpp
    ${PROJECT_SOURCE_DIR}/cli/menu.cpp
    ${PROJECT_SOURCE_DIR}/include/inetsockets_tcp.cpp
    ${PROJECT_SOURCE_DIR}/include/Threadpool.cpp
    ${PROJECT_SOURCE_DIR}/cli/User.cpp
)

target_link_libraries(client
    hiredis
    ncurses
)

add_executable(server
    ${PROJECT_SOURCE_DIR}/ser/serve.cpp
    ${PROJECT_SOURCE_DIR}/ser/chat_serve.cpp
    ${PROJECT_SOURCE_DIR}/include/inetsockets_tcp.cpp
    ${PROJECT_SOURCE_DIR}/include/Threadpool.cpp
    ${PROJECT_SOURCE_DIR}/Database/Database.cpp
)

add_executable(FTP_serve
    ${PROJECT_SOURCE_DIR}/ser/FTP_serve.cpp
    ${PROJECT_SOURCE_DIR}/include/inetsockets_tcp.cpp
    ${PROJECT_SOURCE_DIR}/include/Threadpool.cpp
)

target_link_libraries(server
    hiredis
    ncurses
    ${MYSQL_LIBRARIES}
)

target_link_libraries(FTP_serve
    hiredis
    ncurses
    readline
)

target_link_libraries(client readline)
target_link_libraries(server readline)

target_include_directories(server PUBLIC
    ${MYSQL_INCLUDE_DIRS}
)
