#include "/home/mcy-mcy/文档/chatroom/include/inetsockets_tcp.hpp"
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include "/home/mcy-mcy/文档/chatroom/include/Threadpool.hpp"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <algorithm>
#include <mutex>
#include <queue>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

typedef struct reactargs{
    int cnt;
    int cefd;
    int eventfd;
    pool *handle_recv;
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
            lev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
            if(epoll_ctl(lefd,EPOLL_CTL_ADD,lfd,&lev) == -1){
                perror("epoll_ctl");
                startflag = false;
                return;
            }

            handle_recv = new pool(HANDLE_RECV_NUM);
            reactarr = new reactargs[REACTSIZE];

            return;

        }

    void start(){

        while(startflag){

            int lep_cnt = epoll_wait(lefd,levlist,EPSIZE,-1);
            int cfd = 0;
            struct epoll_event ev;

            if(lep_cnt < 0){
                perror("epoll_wait");
                startflag = false;
                break;
            }
            
            // 处理所有就绪事件
            for(int i = 0; i < lep_cnt; i++) {
                if(levlist[i].data.fd == lfd) {
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
                reactarr[i].handle_recv = handle_recv;
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
                            ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP ;
                            epoll_ctl(pthargs->cefd, EPOLL_CTL_ADD, fd, &ev);
                        }
                        continue;
                    }
                    
                    // 处理连接关闭
                    if(evlist[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
                        close(evlist[i].data.fd);
                        {
                            std::lock_guard<std::mutex> lock(pthargs->queue_mutex);
                            pthargs->cnt--;  // 减少连接计数
                        }
                        continue;
                    }
                    
                    if(evlist[i].events & EPOLLIN) {
                        cout << "react_pthread_cfd : " << evlist[i].data.fd << endl;
                        pthargs->handle_recv->addtask(handle_recv_func,(void*)&evlist[i].data.fd);
                    }
                }
            }

            return nullptr;
        }
        
        static bool reactcmp(const reactargs& a, const reactargs& b) {
            return a.cnt < b.cnt;
        }

        static void *handle_recv_func(void *args){
            int cfd = *(int*)args;
            cout << "handle_recv_cfd : " << cfd << endl;
            char* rec_quest = new char[MAXBUF];
            int n;

            while((n = recv(cfd,rec_quest,MAXBUF,MSG_DONTWAIT)) != 0){
                cout << "n : " << n << endl;
                if(n == -1){
                    if(errno != EAGAIN && errno != EWOULDBLOCK){
                        perror("recv");
                        return nullptr;
                    }else break;               
                } 
            }
            if(n == 0)
                return nullptr;

            json json_quest = nlohmann::json::parse(rec_quest);
            int request = json_quest["request"];

            switch(request){
                case(LOGIN):
                    break;
                case(SIGNIN):{
                    std::string msg = json_quest["message"];
                    const char* test = msg.c_str();
                    int test_len = msg.size();
                    send(cfd,test,test_len,0);
                    break;
                }
            }

            delete[] rec_quest;
            return nullptr;
        }

        int lefd;
        int lfd;
        bool startflag;
        bool creatflag;
        pool *handle_recv;
        reactargs *reactarr;
        uint64_t event_value;
        struct epoll_event lev;
        struct epoll_event levlist[EPSIZE];

};
