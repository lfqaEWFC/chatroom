#include "inetsockets_tcp.hpp"
#include "define.hpp"
#include "Threadpool.hpp"
#include "Database.hpp"
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
    queue<int> pending_fds;  
    mutex queue_mutex;
    unordered_map<int, string>* cfd_to_user;
    unordered_map<string ,int>* user_to_cfd;
    unordered_map<int, string>* cfd_to_buffer;
    unordered_map<string, int>* user_to_group;
    unordered_map<string, string>* user_to_friend;  
} reactargs;

typedef struct handle_recv_args{
    int cfd;
    string json_str;
    bool pri_redis_flag = false;;
    unordered_map<int, string>* cfd_to_user;
    unordered_map<string, int>* user_to_cfd;
    unordered_map<string, string>* user_to_friend;
    unordered_map<string, int>* user_to_group;
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

            database db("localhost", 0, "root", nullptr, "chat_database", "localhost", 6379);
            
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
                if(levlist[i].data.fd == lfd) 
                {
                    while(true) 
                    {
                        cfd = accept(lfd, nullptr, nullptr);
                        if(cfd < 0) {
                            if(errno == EAGAIN || errno == EWOULDBLOCK) 
                                break;
                            perror("accept");
                            continue;
                        }

                        if(creatflag)
                        {
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
        
        delete handle_recv;
        delete[] reactarr;
    }

    private:

        void init_reactors() 
        {
            struct epoll_event ev;
            for(int i = 0; i < REACTSIZE; i++) 
            {
                reactarr[i].cefd = epoll_create(EPSIZE);
                reactarr[i].cnt = 0;
                reactarr[i].eventfd = eventfd(0, EFD_NONBLOCK);
                reactarr[i].handle_recv = handle_recv;
                reactarr[i].cfd_to_user = &cfd_to_user;
                reactarr[i].user_to_cfd = &user_to_cfd;
                reactarr[i].cfd_to_buffer = &cfd_to_buffer;
                reactarr[i].user_to_friend = &user_to_friend;
                reactarr[i].user_to_group = &user_to_group;
                if(reactarr[i].cefd == -1) 
                {
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
        
        int find_least_loaded() 
        {
            int min_idx = 0;
            for(int i = 1; i < REACTSIZE; i++) {
                if(reactarr[i].cnt < reactarr[min_idx].cnt) 
                {
                    min_idx = i;
                }
            }
            return min_idx;
        }
        
        void assign_connection(int cfd, int idx) 
        {
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

                if(n < 0) 
                {
                    perror("epoll_wait");
                    continue;
                }
                
                for(int i = 0; i < n; i++)
                {
                    if(evlist[i].data.fd == pthargs->eventfd) 
                    {
                        uint64_t count;
                        while(read(pthargs->eventfd, &count, sizeof(count)) > 0);
                        
                        std::queue<int> temp_queue;
                        {
                            lock_guard<mutex> lock(pthargs->queue_mutex);
                            temp_queue.swap(pthargs->pending_fds);
                        }
                        
                        while(!temp_queue.empty()) 
                        {
                            int fd = temp_queue.front();
                            temp_queue.pop();
                            
                            struct epoll_event ev;
                            ev.data.fd = fd;
                            ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP ;
                            if (epoll_ctl(pthargs->cefd, EPOLL_CTL_ADD, fd, &ev) == -1) 
                            {
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
                    
                    if(evlist[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) 
                    {
                        close(evlist[i].data.fd);
                        cout << "close : " << evlist[i].data.fd << endl;
                        epoll_ctl(pthargs->cefd, EPOLL_CTL_DEL, evlist[i].data.fd, nullptr);
                        {
                            lock_guard<mutex> lock(pthargs->queue_mutex);
                            auto it = 
                                pthargs->cfd_to_user->find(evlist[i].data.fd);
                            auto del_buffer = 
                                pthargs->cfd_to_buffer->find(evlist[i].data.fd);
                            static thread_local unique_ptr<database> db = nullptr;
                            if(del_buffer != pthargs->cfd_to_buffer->end())
                            {
                                cout << "delete cfd_to_buffer" << endl;
                                pthargs->cfd_to_buffer->erase(del_buffer);
                            }
                            if(it != pthargs->cfd_to_user->end())
                            {
                                string username = it->second;
                                cout << "delete cfd_to_user" << endl;
                                pthargs->cfd_to_user->erase(it);
                                auto it_fd = pthargs->user_to_cfd->find(username);
                                if(it_fd != pthargs->user_to_cfd->end())
                                {
                                    cout << "delete user_to_cfd" << endl;
                                    pthargs->user_to_cfd->erase(it_fd);
                                }
                                auto it_name = pthargs->user_to_friend->find(username);
                                if(it_name != pthargs->user_to_friend->end())
                                {
                                    cout << "delete user_to_friend" << endl;
                                    pthargs->user_to_friend->erase(it_name);
                                }
                                auto it_id = pthargs->user_to_group->find(username);
                                if(it_id != pthargs->user_to_group->end())
                                {
                                    cout << "delete user_to_group" << endl;
                                    pthargs->user_to_group->erase(it_id);
                                }
                                if (!db) 
                                {
                                    db = make_unique<database>(
                                        "localhost", 0, "root", nullptr, 
                                        "chat_database", "localhost", 6379
                                    );
                                    if (!db->is_connected()) 
                                    {
                                        cerr << "React Redis DB connect error" << endl;
                                    }
                                }
                                string redis_key = "offline:logout_time";
                                redisContext *redis = db->get_redis_conn();
                                string timestamp = get_current_mysql_timestamp();
                                const char* argv[] = 
                                    {"HSET", redis_key.c_str(), 
                                    username.c_str(), timestamp.c_str()
                                };
                                size_t argvlen[] = 
                                    {4, redis_key.size(), username.size(), timestamp.size()};
                                redisReply* reply = (redisReply*)redisCommandArgv(
                                    redis, 4, argv, argvlen
                                );
                                if(reply) freeReplyObject(reply);
                                cout << "logout_time change" << endl;
                            }
                            pthargs->cnt--;
                        }
                        continue;
                    }
                    
                    if(evlist[i].events & EPOLLIN) 
                    {
                        while(true)
                        {
                            int n;
                            int cfd = evlist[i].data.fd;
                            char rec_quest[MAXBUF];

                            string& buffer = (*pthargs->cfd_to_buffer)[cfd];

                            while((n = recv(cfd,rec_quest,MAXBUF,MSG_DONTWAIT)) > 0)
                            {
                                buffer.append(rec_quest,n);
                            }
                            
                            if(n == -1)
                            {
                                if(errno != EAGAIN && errno != EWOULDBLOCK)
                                {
                                    close(cfd);
                                    perror("recv");
                                    epoll_ctl(pthargs->cefd, EPOLL_CTL_DEL, cfd, nullptr);
                                    static thread_local unique_ptr<database> db = nullptr;
                                    {
                                        lock_guard<mutex> lock(pthargs->queue_mutex);
                                        pthargs->cnt--;
                                        auto it = pthargs->cfd_to_user->find(cfd);
                                        auto del_buffer = pthargs->cfd_to_buffer->find(cfd);
                                        if(del_buffer != pthargs->cfd_to_buffer->end())
                                            pthargs->cfd_to_buffer->erase(del_buffer);
                                        if(it != pthargs->cfd_to_user->end()){
                                            string username = it->second;
                                            if (!db) 
                                            {
                                                db = make_unique<database>(
                                                    "localhost", 0, 
                                                    "root", nullptr, 
                                                    "chat_database", 
                                                    "localhost", 6379
                                                );
                                                if (!db->is_connected()) 
                                                {
                                                    cerr << "React Redis DB connect error" << endl;
                                                }
                                            }
                                            string redis_key = "offline:logout_time";
                                            redisContext *redis = db->get_redis_conn();
                                            string timestamp = get_current_mysql_timestamp();
                                            const char* argv[] = 
                                            {
                                                "HSET", redis_key.c_str(), 
                                                username.c_str(), timestamp.c_str()
                                            };
                                            size_t argvlen[] = {4, redis_key.size(), username.size(), 
                                                                timestamp.size()};
                                            redisReply* reply = (redisReply*)redisCommandArgv(
                                                redis, 4, argv, argvlen
                                            );
                                            if(reply)
                                                freeReplyObject(reply);
                                            cout << "logout_time change" << endl;
                                            auto it_fd = 
                                                pthargs->user_to_cfd->find(username);
                                            if(it_fd != pthargs->user_to_cfd->end())
                                                pthargs->user_to_cfd->erase(it_fd);
                                            auto it_name = 
                                                pthargs->user_to_friend->find(username);
                                            if(it_name != pthargs->user_to_friend->end())
                                                pthargs->user_to_friend->erase(it_name);
                                            auto it_id = 
                                                pthargs->user_to_group->find(username);
                                            if(it_id != pthargs->user_to_group->end())
                                                pthargs->user_to_group->erase(it_id);
                                            pthargs->cfd_to_user->erase(it);
                                        }
                                    }
                                    break;
                                }              
                            }
                            if(n == 0)
                                break;
                                
                            if (buffer.size() < 4)
                            {
                                cout << "end len: " << buffer.size() <<endl;
                                break;
                            }
            
                            uint32_t net_len;
                            memcpy(&net_len, buffer.data(), 4);
                            uint32_t json_len = ntohl(net_len);
                            cout << "json_len : " <<json_len << endl;
            
                            if (buffer.size() < 4 + json_len) break;
            
                            string json_str = buffer.substr(4, json_len);
                            handle_recv_args *args = new handle_recv_args;

                            json json_quest;
                            try {
                                json_quest = json::parse(json_str);
                            } catch (...) {
                                cerr << "JSON parse error\n";
                                return nullptr;
                            }
                            int request = json_quest["request"];
                            if(json_quest["request"] == PRIVATE_CHAT)
                            {
                                string sender = json_quest["from"];
                                string receiver = json_quest["to"];
                                string message = json_quest["message"];

                                auto it = (*pthargs->cfd_to_user).begin();
                                for (; it != (*pthargs->cfd_to_user).end(); ++it) {
                                    if (it->second == receiver) {
                                        int cfd = it->first;
                                        if((*pthargs->user_to_friend)[receiver] == sender){                   
                                            json send_msg = {
                                                {"sort",MESSAGE},
                                                {"request",PEER_CHAT},
                                                {"sender",sender},
                                                {"receiver",receiver},
                                                {"message",message}
                                            };
                                            sendjson(send_msg,cfd);
                                            break;
                                        }
                                        else{
                                            json send_msg = {
                                                {"sort",MESSAGE},
                                                {"request",NON_PEER_CHAT},
                                                {"message","通知: 好友"+sender+"向您发送了一条新消息"}
                                            };
                                            sendjson(send_msg,cfd);
                                            break;
                                        }
                                    }
                                }
                                if(it == (*pthargs->cfd_to_user).end())
                                    args->pri_redis_flag = true;
                            }
                            if(json_quest["request"] == GROUP_CHAT)
                            {
                                long gid = json_quest["gid"];
                                string message = json_quest["message"];
                                string username = json_quest["username"];

                                static thread_local unique_ptr<database> db = nullptr;
                                if(!db)
                                {
                                    db = make_unique<database>(
                                        "localhost", 0, 
                                        "root", nullptr, 
                                        "chat_database", 
                                        "localhost", 6379
                                    );
                                    if (!db->is_connected()) 
                                    {
                                        cerr << "React Redis DB connect error" << endl;
                                    }
                                }

                                string safe_username = db->escape_mysql_string_full(username);
                                MYSQL_RES* res = db->query_sql(
                                    "SELECT username FROM group_members "
                                    "WHERE group_id = " + to_string(gid) + " "
                                    "AND username <> '" + safe_username + "' "
                                    "AND status != 0;"
                                );

                                MYSQL_ROW row;
                                while ((row = mysql_fetch_row(res)) != nullptr)
                                {
                                    string receiver = row[0];
                                    auto it = (*pthargs->user_to_cfd).find(receiver);
                                    int cfd = (it != (*pthargs->user_to_cfd).end()) ? it->second : 0;
                                    if (cfd == 0)
                                        continue;

                                    json send_json;
                                    auto it_group = (*pthargs->user_to_group).find(receiver);
                                    if (it_group == (*pthargs->user_to_group).end() || it_group->second != gid)
                                    {
                                        send_json = {
                                            {"sort", MESSAGE},
                                            {"request", NOT_PEER_GROUP},
                                            {"message", 
                                            "通知: gid为 " + to_string(gid) + " 的群组有一条新消息"}
                                        };
                                    }
                                    else
                                    {
                                        send_json = {
                                            {"sort", MESSAGE},
                                            {"request", PEER_GROUP},
                                            {"sender", username},
                                            {"message", message}
                                        };
                                    }
                                    sendjson(send_json, cfd);
                                }
                                db->free_result(res);
                            }

                            args->cfd = evlist[i].data.fd;
                            args->cfd_to_user = pthargs->cfd_to_user;
                            args->user_to_cfd = pthargs->user_to_cfd;
                            args->user_to_friend = pthargs->user_to_friend;
                            args->user_to_group = pthargs->user_to_group;
                            args->json_str = json_str;               
                            buffer.erase(0, 4 + json_len);
                            pthargs->handle_recv->addtask(handle_recv_func,args);
                        }
                    }
                }
            }

            return nullptr;
        }
        
        static bool reactcmp(const reactargs& a, const reactargs& b) {
            return a.cnt < b.cnt;
        }

        static void *handle_recv_func(void *args){
            cout << "recv_func" << endl;
            handle_recv_args *new_args = (handle_recv_args*)args; 
            static thread_local unique_ptr<database> db = nullptr;

            if (!db) {
                db = make_unique<database>(
                    "localhost", 0, "root", nullptr, "chat_database", "localhost", 6379
                );
                if (!db->is_connected()) {
                    cerr << "Connect Database Error" << endl;
                    json send_json{
                        {"sort",ERROR},
                        {"reflact","数据库连接失败..."},
                    };
                    sendjson(send_json,new_args->cfd);
                    db = nullptr;
                    delete new_args;
                    return nullptr;
                }
            }

            json json_quest;
            try {
                json_quest = json::parse(new_args->json_str);
            } catch (...) {
                cerr << "JSON parse error\n";
                return nullptr;
            }

            int request = json_quest["request"];

            switch(request){
                case(LOGIN):{
                    json *reflact = new json;
                    handle_login(json_quest,db,reflact,new_args->cfd_to_user);             
                    sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(FORGET_PASSWORD):{
                    json *reflact = new json;
                    handle_forget_password(json_quest,db,reflact);                                
                    sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(CHECK_ANS):{
                    json *reflact = new json;
                    handle_check_answer(json_quest,db,reflact);
                    sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(IN_ONLINE):{
                    string username = json_quest["username"];
                    cout << "add cfd_to_user: " << username << endl;
                    cout << "add user_to_cfd: " << username << endl;
                    (*new_args->cfd_to_user)[new_args->cfd] = username;
                    (*new_args->user_to_cfd)[username] = new_args->cfd;
                    break;
                }
                case(SIGNIN):{
                    if(handle_signin(json_quest,db)){
                        json send_json{
                            {"sort",REFLACT},
                            {"request",SIGNIN},
                            {"reflact","注册成功，请进行下一步操作"},
                        };
                        sendjson(send_json,new_args->cfd);
                        break;
                    }else{
                        json send_json{
                            {"sort",REFLACT},
                            {"request",SIGNIN},
                            {"reflact","用户名重复，请重新注册"},
                        };
                        sendjson(send_json,new_args->cfd);
                        break;
                    }
                }
                case(LOGOUT):{
                    json *reflact = new json;
                    handle_logout(json_quest,db,reflact,
                        new_args->cfd_to_user,new_args->user_to_cfd);                         
                    sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(BREAK):{
                    json *reflact = new json;
                    handle_break(json_quest,db,reflact);
                    sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(ADD_FRIEND):{
                    json *reflact = new json;
                    handle_add_friend(json_quest,new_args->cfd_to_user,db,reflact);
                    sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(GET_OFFLINE_MSG):{
                    json *reflact = new json;
                    if(handle_get_offline(json_quest,db,reflact))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(GET_FRIEND_REQ):{
                    json *reflact = new json;
                    if(handle_get_friend(json_quest,db,reflact)){
                        sendjson(*reflact,new_args->cfd);
                        cout << "get" << endl;
                    }
                    delete reflact;
                    break;
                }
                case(DEAL_FRI_REQ):{
                    json *reflact = new json;
                    if(handle_deal_friend(json_quest,db,reflact,*new_args->user_to_cfd))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(CHAT_NAME):{
                    json *reflact = new json;
                    if(handle_chat_name(json_quest,db,reflact,new_args->user_to_friend))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(GET_HISTORY_PRI):{
                    json *reflact = new json;
                    if(handle_history_pri(json_quest,db,reflact,*new_args->user_to_friend))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;    
                }
                case(PRIVATE_CHAT):{
                    json *reflact = new json;
                    if(handle_private_chat(json_quest,db,reflact,new_args->user_to_friend,
                        *new_args->cfd_to_user,new_args->pri_redis_flag))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(DEL_PEER):{
                    handle_del_peer(json_quest,db,new_args->user_to_friend);
                    break;
                }
                case(ADD_BLACKLIST):{
                    json *reflact = new json;
                    if(handle_add_black(json_quest,reflact,db,
                        *new_args->user_to_cfd,new_args->user_to_friend))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(REMOVE_BLACKLIST):{
                    json *reflact = new json;
                    if(handle_rem_black(json_quest,reflact,db,*new_args->user_to_cfd))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(CHECK_FRIEND):{
                    json *reflact = new json;
                    if(handle_check_friend(json_quest,reflact,db,new_args->cfd_to_user))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(DELETE_FRIEND):{
                    json *reflact = new json;
                    if(handle_del_friend(json_quest,reflact,db,
                        *new_args->user_to_cfd,new_args->user_to_friend))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(ADD_FILE):{
                    json *reflact = new json;
                    if(handle_add_file(json_quest,reflact,db,
                        *new_args->cfd_to_user,*new_args->user_to_friend,
                        *new_args->user_to_cfd,*new_args->user_to_group))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(SHOW_FILE):{
                    json *reflact = new json;
                    if(handle_show_file(json_quest,reflact,db))
                        sendjson(*reflact,new_args->cfd);   
                    delete reflact;
                    break;
                }
                case(CREATE_GROUP):{
                    json *reflact = new json;
                    if(handle_create_group(json_quest,reflact,db))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(SEL_GROUP):{
                    json *reflact = new json;
                    if(handle_select_group(json_quest,reflact,db))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(ADD_GROUP):{
                    json *reflact = new json;
                    if(handle_add_group(json_quest,reflact,db,*new_args->user_to_cfd))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(DEAL_ADDGROUP):{
                    json *reflact = new json;
                    if(deal_add_group(json_quest,reflact,db))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(COMMIT_ADD):{
                    json *reflact = new json;
                    if(handle_commit_add(json_quest,reflact,db,*new_args->user_to_cfd))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                } 
                case(SHOW_GROUP):{
                    json *reflact = new json;
                    if(handle_show_group(json_quest,reflact,db))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(GROUP_NAME):{
                    json *reflact = new json;
                    if(handle_group_name(json_quest,reflact,db,new_args->user_to_group))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(GROUP_HISTORY):{
                    json *reflact = new json;
                    if(handle_group_history(json_quest,reflact,db))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                } 
                case(GROUP_CHAT):{
                    json *reflact = new json;
                    if(handle_group_chat(json_quest,reflact,db,
                        *new_args->user_to_cfd,*new_args->user_to_group))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(GROUP_END):{
                    json *reflact = new json;
                    if(handle_group_end(json_quest,reflact,db,new_args->user_to_group))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(SHOW_MEMBER):{
                    json *reflact = new json;
                    if(handle_show_member(json_quest,reflact,db))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }  
                case(ADD_ADMIN):{
                    json *reflact = new json;
                    if(handle_add_admin(json_quest,reflact,db,*new_args->user_to_cfd))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(SHOW_FRIEND):{
                    json *reflact = new json;
                    if(handle_show_friend(json_quest,reflact,db))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }   
                case(ADD_GROUP_MEM):{
                    json *reflact = new json;
                    if(handle_add_member(json_quest,reflact,db,*new_args->user_to_cfd))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(BREAK_GROUP):{
                    json *reflact = new json;
                    if(handle_break_group(json_quest,reflact,db,
                        *new_args->user_to_cfd,new_args->user_to_group))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(KILL_GROUP_USER):{
                    json *reflact = new json;
                    if(handle_kill_user(json_quest,reflact,db,
                        *new_args->user_to_cfd,new_args->user_to_group))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
                case(DELETE_GROUP):{
                    json *reflact = new json;
                    if(handle_del_group(json_quest,reflact,db,
                        *new_args->user_to_cfd,new_args->user_to_group))
                        sendjson(*reflact,new_args->cfd);
                    delete reflact;
                    break;
                }
            }

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
        unordered_map<int, string> cfd_to_user;
        unordered_map<string ,int> user_to_cfd;
        unordered_map<int, string> cfd_to_buffer;
        unordered_map<string,string> user_to_friend;
        unordered_map<string, int> user_to_group;
};
