cmake_minimum_required (VERSION 3.10)

project (chatroom)

add_executable(client
    ${PROJECT_SOURCE_DIR}/cli/chat_client.cpp
    ${PROJECT_SOURCE_DIR}/cli/menu.cpp
    ${PROJECT_SOURCE_DIR}/include/inetsockets_tcp.cpp
    ${PROJECT_SOURCE_DIR}/include/Threadpool.cpp
)

# Link libraries for cli
target_link_libraries(client
    hiredis
    ncurses
)

# Add the executable for server
add_executable(server
    ${PROJECT_SOURCE_DIR}/ser/chat_serve.cpp
    ${PROJECT_SOURCE_DIR}/include/inetsockets_tcp.cpp
    ${PROJECT_SOURCE_DIR}/include/Threadpool.cpp
)

# Link libraries for cli
target_link_libraries(server
    hiredis
    ncurses
)
