#include "/home/mcy-mcy/文档/chatroom/include/inetsockets_tcp.hpp"
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include "/home/mcy-mcy/文档/chatroom/include/Threadpool.hpp"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <algorithm>
#include <mutex>
#include <queue>

typedef struct reactargs{
    int cnt;
    int cefd;
    int eventfd;
    pthread_t pthreact;
    std::queue<int> pending_fds;  // 待处理连接队列
    std::mutex queue_mutex;       // 队列锁
} reactargs;

class serve{

    public:

        serve(const char *portnum):
        lefd(0),lfd(0),
        startflag(true),creatflag(true),
        event_value(1){

            lfd = inetlisten(portnum);
            cout << portnum << endl;
            if(set_nonblocking(lfd) == -1){
                perror("set_nonblocking");
                startflag = false;
                return;
            }

            lefd = epoll_create(EPSIZE);
            if(lefd == -1){
                perror("epoll_create");
                startflag = false;
                return;
            }
            lev.data.fd = lfd;
            lev.events = EPOLLIN | EPOLLET;  // 添加EPOLLIN
            if(epoll_ctl(lefd,EPOLL_CTL_ADD,lfd,&lev) == -1){
                perror("epoll_ctl");
                startflag = false;
                return;
            }

            reactarr = new reactargs[REACTSIZE];

            return;

        }

    void start(){

        while(startflag){

            int lep_cnt = epoll_wait(lefd,levlist,EPSIZE,-1);
            int cfd = 0;
            struct epoll_event ev;

            if(lep_cnt < 0){  // 修改错误检查
                perror("epoll_wait");
                startflag = false;
                break;
            }
            
            // 处理所有就绪事件
            for(int i = 0; i < lep_cnt; i++) {
                if(levlist[i].data.fd == lfd) {
                    // 边缘触发模式下循环accept
                    while(true) {
                        cfd = accept(lfd, nullptr, nullptr);
                        if(cfd < 0) {
                            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                                // 所有连接已处理
                                break;
                            }
                            perror("accept");
                            continue;
                        }

                        set_nonblocking(cfd);

                        if(creatflag){
                            init_reactors();
                            creatflag = false;
                        }

                        int min_idx = find_least_loaded();
                        assign_connection(cfd, min_idx);
                    }
                }
            }
        }

        return;
    }

    ~serve(){
        for(int i = 0; i < REACTSIZE; i++) {
            close(reactarr[i].cefd);
            close(reactarr[i].eventfd);
            pthread_cancel(reactarr[i].pthreact);
        }
        
        delete[] reactarr;
    }

    private:

        void init_reactors() {
            struct epoll_event ev;
            for(int i = 0; i < REACTSIZE; i++) {
                reactarr[i].cefd = epoll_create(EPSIZE);
                reactarr[i].cnt = 0;
                reactarr[i].eventfd = eventfd(0, EFD_NONBLOCK);
                if(reactarr[i].cefd == -1) {
                    perror("epoll_create");
                    startflag = false;
                    return;
                }
                ev.data.fd = reactarr[i].eventfd;
                ev.events = EPOLLIN | EPOLLET;
                epoll_ctl(reactarr[i].cefd, EPOLL_CTL_ADD, reactarr[i].eventfd, &ev);
                pthread_create(&reactarr[i].pthreact, nullptr, react_pthread, &reactarr[i]);
            }
        }
        
        int find_least_loaded() {
            int min_idx = 0;
            for(int i = 1; i < REACTSIZE; i++) {
                if(reactarr[i].cnt < reactarr[min_idx].cnt) {
                    min_idx = i;
                }
            }
            return min_idx;
        }
        
        void assign_connection(int cfd, int idx) {
            reactargs& target = reactarr[idx];
            {
                std::lock_guard<std::mutex> lock(target.queue_mutex);
                bool was_empty = target.pending_fds.empty();
                target.pending_fds.push(cfd);
                target.cnt++;  // 增加连接计数
                
                if(was_empty) {
                    // 唤醒线程
                    write(target.eventfd, &event_value, sizeof(event_value));
                }
            }
        }

        static void *react_pthread(void *args){

            reactargs *pthargs = (reactargs*)args;
            struct epoll_event evlist[EPSIZE];

            while(true){

                int n = epoll_wait(pthargs->cefd, evlist, EPSIZE, -1);

                if(n < 0) {
                    perror("epoll_wait");
                    continue;
                }
                
                for(int i = 0; i < n; i++){
                    // 处理唤醒事件
                    if(evlist[i].data.fd == pthargs->eventfd) {
                        // 完全读取eventfd
                        uint64_t count;
                        while(read(pthargs->eventfd, &count, sizeof(count)) > 0);
                        
                        // 处理所有待处理连接
                        std::queue<int> temp_queue;
                        {
                            std::lock_guard<std::mutex> lock(pthargs->queue_mutex);
                            temp_queue.swap(pthargs->pending_fds);
                        }
                        
                        while(!temp_queue.empty()) {
                            int fd = temp_queue.front();
                            temp_queue.pop();
                            
                            // 添加到epoll
                            struct epoll_event ev;
                            ev.data.fd = fd;
                            ev.events = EPOLLIN | EPOLLET;
                            epoll_ctl(pthargs->cefd, EPOLL_CTL_ADD, fd, &ev);
                        }
                        continue;
                    }
                    
                    // 处理连接关闭
                    if(evlist[i].events & (EPOLLHUP | EPOLLERR)) {
                        close(evlist[i].data.fd);
                        {
                            std::lock_guard<std::mutex> lock(pthargs->queue_mutex);
                            pthargs->cnt--;  // 减少连接计数
                        }
                        continue;
                    }
                    
                    // 处理可读事件 (测试用)
                    if(evlist[i].events & EPOLLIN) {
                        char buffer[1024];
                        ssize_t len = recv(evlist[i].data.fd, buffer, sizeof(buffer), 0);
                        if(len > 0) {
                            // 简单回显
                            send(evlist[i].data.fd, "hello", sizeof("hello"), 0);
                        } else if(len == 0) {
                            // 客户端关闭连接
                            close(evlist[i].data.fd);
                            {
                                std::lock_guard<std::mutex> lock(pthargs->queue_mutex);
                                pthargs->cnt--;
                            }
                        }
                    }
                }
            }

            return nullptr;
        }
        
        static bool reactcmp(const reactargs& a, const reactargs& b) {
            return a.cnt < b.cnt;
        }

        int lefd;
        int lfd;
        uint64_t event_value;
        bool startflag;
        bool creatflag;
        reactargs *reactarr;
        struct epoll_event lev;
        struct epoll_event levlist[EPSIZE];

};
