#include "/home/mcy-mcy/文档/chatroom/include/inetsockets_tcp.hpp"
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include "/home/mcy-mcy/文档/chatroom/include/Threadpool.hpp"
#include "/home/mcy-mcy/文档/chatroom/Database/Database.hpp"
#include "serve.hpp"
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
    queue<int> pending_fds;  // 待处理连接队列
    mutex queue_mutex;       // 队列锁
} reactargs;

typedef struct handle_recv_args{
    int cfd;
    int *reactcnt;
    mutex *react_quene_mutex;
}handle_recv_args;

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
            
            for(int i = 0; i < lep_cnt; i++) {
                if(levlist[i].data.fd == lfd) {
                    while(true) {
                        cfd = accept(lfd, nullptr, nullptr);
                        if(cfd < 0) {
                            if(errno == EAGAIN || errno == EWOULDBLOCK) {
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
            pthread_cancel(reactarr[i].pthreact);
            pthread_join(reactarr[i].pthreact, nullptr); 
            close(reactarr[i].eventfd);       
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
                lock_guard<mutex> lock(target.queue_mutex);
                bool was_empty = target.pending_fds.empty();
                target.pending_fds.push(cfd);
                target.cnt++;
                
                if(was_empty) {
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

                    if(evlist[i].data.fd == pthargs->eventfd) {
                        uint64_t count;
                        while(read(pthargs->eventfd, &count, sizeof(count)) > 0);
                        
                        std::queue<int> temp_queue;
                        {
                            lock_guard<mutex> lock(pthargs->queue_mutex);
                            temp_queue.swap(pthargs->pending_fds);
                        }
                        
                        while(!temp_queue.empty()) {
                            int fd = temp_queue.front();
                            temp_queue.pop();
                            
                            struct epoll_event ev;
                            ev.data.fd = fd;
                            ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP ;
                            if (epoll_ctl(pthargs->cefd, EPOLL_CTL_ADD, fd, &ev) == -1) {
                                perror("epoll_ctl ADD");
                                close(fd);
                                {
                                    lock_guard<mutex> lock(pthargs->queue_mutex);
                                    pthargs->cnt--;
                                }
                                continue;
                            }
                        }
                        continue;
                    }
                    
                    if(evlist[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
                        close(evlist[i].data.fd);
                        epoll_ctl(pthargs->cefd, EPOLL_CTL_DEL, evlist[i].data.fd, nullptr);
                        {
                            lock_guard<mutex> lock(pthargs->queue_mutex);
                            pthargs->cnt--;
                        }
                        continue;
                    }
                    
                    if(evlist[i].events & EPOLLIN) {
                        handle_recv_args *args = new handle_recv_args;
                        args->cfd = evlist[i].data.fd;
                        args->reactcnt = &pthargs->cnt;
                        args->react_quene_mutex = &pthargs->queue_mutex;
                        pthargs->handle_recv->addtask(handle_recv_func,args);
                    }
                }
            }

            return nullptr;
        }
        
        static bool reactcmp(const reactargs& a, const reactargs& b) {
            return a.cnt < b.cnt;
        }

        static void *handle_recv_func(void *args){
            handle_recv_args *new_args = (handle_recv_args*)args;
            char* rec_quest = new char[MAXBUF];
            static thread_local database* db = nullptr;

            if (!db) {
                db = new database("localhost", 0, "root", nullptr, "chat_database", "localhost", 6379);
                if (!db->is_connected()) {
                    cerr << "Connect Database Error" << endl;
                    json sendjson{
                        {"sort",ERROR},
                        {"reflact","数据库连接失败..."},
                    };
                    string json_str = sendjson.dump();
                    const char* data = json_str.c_str(); 
                    size_t data_len = json_str.size();
                    char *recvbuf = new char[MAXBUF]; 
                    send(new_args->cfd,data,data_len,0);
                    delete db;
                    db = nullptr;
                    delete new_args;
                    return nullptr;
                }
            }

            int n;
            while((n = recv(new_args->cfd,rec_quest,MAXBUF,MSG_DONTWAIT)) != 0){
                if(n == -1){
                    if(errno != EAGAIN && errno != EWOULDBLOCK){
                        perror("recv");
                        close(new_args->cfd);
                        {
                            lock_guard<mutex> lock(*new_args->react_quene_mutex);
                            (*new_args->reactcnt)--;
                        }
                        delete new_args;
                        delete[] rec_quest;
                        return nullptr;
                    }else break;               
                } 
            }
            if(n == 0){
                close(new_args->cfd);
                {
                    lock_guard<mutex> lock(*new_args->react_quene_mutex);
                    (*new_args->reactcnt)--;
                }
                delete new_args;
                delete[] rec_quest;
                return nullptr;
            }
                

            json json_quest = nlohmann::json::parse(rec_quest);
            int request = json_quest["request"];

            switch(request){
                case(LOGIN):
                    break;
                case(SIGNIN):
                    if(handle_signin(json_quest,db)){
                        json sendjson{
                            {"sort",REFLACT},
                            {"reflact","注册成功，请进行下一步操作"},
                        };
                        string json_str = sendjson.dump();
                        const char* data = json_str.c_str(); 
                        size_t data_len = json_str.size();
                        char *recvbuf = new char[MAXBUF]; 
                        send(new_args->cfd,data,data_len,0);
                        break;
                    }else{
                        json sendjson{
                            {"sort",REFLACT},
                            {"reflact","注册失败，请重新注册"},
                        };
                        string json_str = sendjson.dump();
                        const char* data = json_str.c_str(); 
                        size_t data_len = json_str.size();
                        char *recvbuf = new char[MAXBUF]; 
                        send(new_args->cfd,data,data_len,0);
                        break;
                    }
            }

            delete[] rec_quest;
            delete new_args;

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
