#ifndef FTP_HPP
#include"inetsockets_tcp.hpp"
#include"Threadpool.hpp"
#include"define.hpp"
#include<fcntl.h>
#include<sys/epoll.h>
#include<iostream>
#include<string.h>
#include<dirent.h>
#include<sys/stat.h>
#include<sys/sendfile.h>
#include<nlohmann/json.hpp>
#include <time.h>

using json = nlohmann::json;

using namespace std;

#define PORTNUM "2100"
#define EPSIZE 1024
#define CHUNK_SIZE (1024*1024)

typedef struct data_args
{
    int fd = 0;
    int data_num;
    char clientnum[10];
    string retr_filename;
    string  stor_filename;
    off_t stor_filesize;
    bool stor_flag = false;
    bool retr_flag = false;
    int ctrl_pair_fd = 0;
    int stor_filefd = 0;
    data_args *next;
    data_args *data_args_list = nullptr;
    unordered_map<int,data_args*>data_pairs;  
}data_args;

typedef struct ctrl_args
{
    int fd = 0;
    int epfd = 0;
    pool *data_pool;
    char clientnum[10];
    string buffer;
    string json_str;
    ctrl_args *next = nullptr;
    data_args *data_args_list = nullptr;
    unordered_map<int,data_args*>data_pairs;
    unordered_map<int,time_t>* cfd_to_heart; 
}ctrl_args;


class FTP
{
    public:

    FTP(ssize_t input_control_threads,ssize_t input_data_threads):
    control_threads(input_control_threads),
    data_threads(input_data_threads),
    listen_control_fd(-1),connect_fd(-1),epfd(-1),workfd_num(0),
    running(false)
    {
        ctrl_pool = new pool(control_threads);
        data_pool = new pool(data_threads);
        pthread_create(&heart_pthread,nullptr,heart_beat_func,this);
    }
    
    void init(){

        listen_control_fd = inetlisten(PORTNUM);
        if(listen_control_fd == -1){
            perror("inetlisten");
            return;
        }
        if(set_nonblocking(listen_control_fd) == -1)
            return;
        epfd = epoll_create(EPSIZE);
        if(epfd == -1){
            perror("epoll_creat");
            return;
        }
        ev.data.fd = listen_control_fd;
        ev.events = EPOLLIN | EPOLLET;
        if(epoll_ctl(epfd,EPOLL_CTL_ADD,listen_control_fd,&ev) == -1){
            perror("epoll_ctl");
            return;
        }
        running = true;

        return;     
    }

    void start()
    {
        while(running){
            workfd_num = epoll_wait(epfd,evlist,EPSIZE,-1);
            if(workfd_num == -1){
                perror("epoll_wait");
                break;
            }
            if(handle_sort(evlist,workfd_num) == -1){
                perror("handle");
                break;
            }
        }
    }

    ~FTP()
    {
        while(ctrl_args_list!= NULL){
            ctrl_args *prev = ctrl_args_list;
            ctrl_args_list = ctrl_args_list->next;
            free(prev);
        }
        while(data_args_list!=NULL){
            data_args *prev = data_args_list;
            data_args_list = data_args_list->next;
            free(prev);
        }
        free(data_pool);
        free(ctrl_pool);
    }

    private:

    int listen_control_fd,connect_fd;
    int epfd;
    int workfd_num;
    int client_num = 100000;
    int data_num = 200000;
    bool running;
    pool* ctrl_pool;
    pool* data_pool;
    ssize_t control_threads;
    ssize_t data_threads;
    pthread_t heart_pthread;
    data_args *data_args_list;
    ctrl_args *ctrl_args_list;
    struct epoll_event ev;
    struct epoll_event evlist[EPSIZE];
    unordered_map<int,time_t> cfd_to_heart;

    static void* heart_beat_func(void* args)
    {
        FTP* srv = static_cast<FTP*>(args);
        cout << srv->running << endl;
        while(srv->running)
        {
            auto it = srv->cfd_to_heart.begin();
            for(;it != srv->cfd_to_heart.end();it++)
            {
                int cfd = it->first;
                time_t now = time(nullptr);
                time_t timestamp = it->second;
                int time = now - timestamp;
                if(time > 30)  
                    close(cfd);
                json send_json = {
                    {"sort",REFLACT},
                    {"request",PING}
                };
                sendjson(send_json,cfd);
            }
            sleep(10);
        }
        return nullptr;
    }

    static int set_nonblocking(int fd) 
    {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
            perror("fcntl F_GETFL");
            return -1;
        }
        
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("fcntl F_SETFL");
            return -1;
        }
        return 0;
    }

    int handle_sort(epoll_event *evlist,int workfd_num)
    {
        for(int i=0;i<workfd_num;i++){
            if (evlist[i].events & (EPOLLRDHUP | EPOLLERR)) 
            {
                bool de_flag = true;
                close(evlist[i].data.fd);
                epoll_ctl(epfd, EPOLL_CTL_DEL, evlist[i].data.fd, NULL);
                data_args *data_prev = nullptr;
                data_args *de_data = data_args_list;
                ctrl_args *ctrl_prev = nullptr;
                ctrl_args *de_ctrl = ctrl_args_list;
                while(de_data != NULL)
                {
                    if(de_data->fd == evlist[i].data.fd){
                        auto it = de_data->data_pairs.begin();
                        for(;it!= de_data->data_pairs.end();it++){
                            if(it->second->fd == de_data->fd){
                                de_data->data_pairs.erase(it);
                                cout << "erasd de_data" << endl;
                                break;
                            }
                        }
                        if(data_prev){
                            data_prev->next = de_data->next;
                            free(de_data);
                            de_flag = false;
                            cout << "close : " << evlist[i].data.fd << endl;
                            break;
                        }
                        data_prev = de_data;
                        de_data = de_data->next;
                        free(data_prev);
                        data_args_list = de_data;
                        de_flag = false;
                        cout << "close : " << evlist[i].data.fd << endl;
                        break;
                    }
                    data_prev = de_data;
                    de_data = de_data->next;
                }
                while(de_ctrl != NULL && de_flag){
                    if(de_ctrl->fd == evlist[i].data.fd){
                        if(ctrl_prev){
                            ctrl_prev->next = de_ctrl->next;
                            free(de_ctrl);
                            cout << "close : " << evlist[i].data.fd << endl;
                            break;
                        }
                        ctrl_prev = de_ctrl;
                        de_ctrl = de_ctrl->next;
                        free(ctrl_prev);
                        ctrl_args_list = de_ctrl;
                        cout << "close : " << evlist[i].data.fd << endl;
                        break;
                    }
                    ctrl_prev = de_ctrl;
                    de_ctrl = de_ctrl->next;
                }
                continue;
            }
            cout << "epoll_in: " << evlist[i].data.fd << endl;
            if(evlist[i].data.fd == listen_control_fd){
                if(handle_accept(listen_control_fd,false) == -1)
                    return -1;
                continue;
            }
            else
            {
                char* portnum;
                portnum = new char[MAXBUF];
                strcpy(portnum,isportnum(evlist[i].data.fd));

                if(portnum == NULL)
                    return -1;
                if(strcmp(portnum,"2100") == 0)
                {
                    ssize_t n;
                    ctrl_args *new_arg;
                    char recvbuf[MAXBUF];

                    new_arg = ctrl_args_list;
                    while(new_arg != NULL){
                        if(new_arg->fd == evlist[i].data.fd)
                            break;
                        new_arg = new_arg->next;
                    }
                    if(new_arg == NULL){
                        cout << "can not find ctrl_args" << endl;
                        return -1;
                    }

                    while ((n = recv(new_arg->fd, recvbuf, MAXBUF, MSG_DONTWAIT)) > 0)
                    {
                        new_arg->buffer.append(recvbuf, n);
                    }
                    if (n == -1)
                    {
                        if (errno != EAGAIN && errno != EWOULDBLOCK)
                        {
                            perror("recv");
                            return -1;
                        }
                    }
            
                    while (new_arg->buffer.size() >= 4)
                    {
            
                        uint32_t net_len;
                        memcpy(&net_len, new_arg->buffer.data(), 4);
                        uint32_t msg_len = ntohl(net_len);
                        if (msg_len == 0)
                        {
                            cerr << "异常消息长度: " << msg_len << endl;
                            new_arg->buffer.clear();
                            break;
                        }
                        if (new_arg->buffer.size() < 4 + msg_len)
                            break;
                        new_arg->json_str = new_arg->buffer.substr(4, msg_len);
                        new_arg->buffer.erase(0, 4 + msg_len);
                        ctrl_pool->addtask(ctrl_fun,(void*)new_arg);
                        continue;
                    }
                }
                else
                {
                    if(atoi(portnum)<=41024 && atoi(portnum)>=1024){              
                        int optval = 0;
                        socklen_t len = sizeof(optval);
                        if(getsockopt(evlist[i].data.fd, SOL_SOCKET, SO_ACCEPTCONN, &optval, &len) == -1) {
                            perror("getsockopt SO_ACCEPTCONN");
                                return -1;
                            }
                        if(optval) {
                            if(handle_accept(evlist[i].data.fd,true) == -1)
                                return -1;
                            continue;  
                        }else{
                            data_args *data_inargs;
                            data_inargs = data_args_list;
                            while(data_inargs != NULL){
                                if(data_inargs->fd == evlist[i].data.fd)
                                    break;
                                data_inargs = data_inargs->next;
                            }
                            if(data_inargs == NULL){
                                cout << "can not find data_args" << endl;
                                return -1;
                            }
                            cout << "addtask" << endl;
                            data_pool->addtask(data_fun,data_inargs);
                            continue;
                        }
                    }else{
                        cout << "error_sort" << endl;
                        return -1;
                    }
                }
            }
        }
        return 0;
    }

    int handle_accept(int listen_fd,bool data_flag)
    {
        connect_fd = accept(listen_fd,NULL,NULL);
        cout << "open : " << connect_fd << endl;
        if(connect_fd == -1){
            perror("accept");
            return -1;
        }
        if(data_flag)
            ev.events = EPOLLIN|EPOLLONESHOT|EPOLLRDHUP;   
        else
            ev.events = EPOLLIN|EPOLLET|EPOLLRDHUP;
        ev.data.fd = connect_fd;
        if(epoll_ctl(epfd,EPOLL_CTL_ADD,connect_fd,&ev) == -1){
            perror("epoll_ctl");
            return -1;
        };

        if(data_flag){
            
            char input[10];
            if(data_args_list == NULL){
                data_args_list = new data_args;
                data_args_list->fd = connect_fd;
                data_args_list->next = NULL;
                data_args_list->data_args_list = data_args_list;
                cout << "recv:pasv-client_num" << endl;
                recv(connect_fd,input,10,0);
                cout << input << endl;
                strcpy(data_args_list->clientnum,input);
                ctrl_args *ctrl_pair = ctrl_args_list; 
                while(ctrl_pair != nullptr){
                    if(strcmp(ctrl_pair->clientnum,data_args_list->clientnum) == 0){
                        char send_data_num[10];
                        ctrl_pair->data_pairs[data_num] = data_args_list;
                        data_args_list->data_num = data_num;
                        sprintf(send_data_num,"%d",data_num);
                        send(connect_fd,send_data_num,strlen(send_data_num)+1,0);
                        cout << "send-" << send_data_num << endl;
                        data_args_list->ctrl_pair_fd = ctrl_pair->fd;
                        data_args_list->data_pairs = ctrl_pair->data_pairs;
                        cout << "pair success" << endl;
                        data_num++;
                        break;
                    }
                    ctrl_pair = ctrl_pair->next;
                }          
            }else{
                ctrl_args *ctrl_pair = ctrl_args_list;
                data_args *data_add = new data_args;
                data_args *data_tail;
                data_add->fd = connect_fd;
                data_add->next = NULL;
                data_add->data_args_list = data_args_list;
                cout << "recv:pasv-client_num" << endl;
                recv(connect_fd,input,10,0);
                cout << input << endl;
                strcpy(data_add->clientnum,input);
                while(ctrl_pair != nullptr){
                    if(strcmp(ctrl_pair->clientnum,data_add->clientnum) == 0){
                        char send_data_num[10];
                        ctrl_pair->data_pairs[data_num] = data_add;
                        data_add->data_num = data_num;            
                        sprintf(send_data_num,"%d",data_num);
                        send(connect_fd,send_data_num,strlen(send_data_num),0);
                        data_add->ctrl_pair_fd = ctrl_pair->fd;
                        data_args_list->data_pairs = ctrl_pair->data_pairs;
                        cout << "pair success : " << data_add->fd << "," << data_add->clientnum << "," << data_num << endl;
                        data_num++;
                        break;
                    }
                    ctrl_pair = ctrl_pair->next;
                }                                    
                data_tail = data_args_list;
                while(data_tail->next != NULL){
                    data_tail = data_tail->next;
                }
                data_tail->next = data_add;
            }

            ctrl_args *chag_args = ctrl_args_list;
            while(chag_args != NULL){
                chag_args->data_args_list = data_args_list;
                chag_args = chag_args->next;
            }

            epoll_ctl(epfd, EPOLL_CTL_DEL, listen_fd, nullptr);
            close(listen_fd);

        }else{
            cfd_to_heart[connect_fd] = time(nullptr);
            cout << "Heartbeat received from cfd " << connect_fd << endl;
            if(ctrl_args_list == NULL){
                ctrl_args_list = new ctrl_args;
                ctrl_args_list->fd = connect_fd;
                ctrl_args_list->epfd = epfd;
                ctrl_args_list->data_pool = data_pool;
                ctrl_args_list->cfd_to_heart = &cfd_to_heart;
                ctrl_args_list->next = NULL;
                sprintf(ctrl_args_list->clientnum,"%d",++client_num);
                send(connect_fd,ctrl_args_list->clientnum,strlen(ctrl_args_list->clientnum),0);                
            }else{
                ctrl_args *ctrl_add = new ctrl_args;
                ctrl_args *ctrl_tail;
                ctrl_add->fd = connect_fd;
                ctrl_add->next = NULL;  
                ctrl_add->epfd = epfd;
                ctrl_add->data_pool = data_pool;
                ctrl_add->cfd_to_heart = &cfd_to_heart;
                sprintf(ctrl_add->clientnum,"%d",++client_num);
                send(connect_fd,ctrl_add->clientnum,strlen(ctrl_add->clientnum),0);                
                ctrl_tail = ctrl_args_list;
                while(ctrl_tail->next != NULL){
                    ctrl_tail = ctrl_tail->next;
                }
                ctrl_tail->next = ctrl_add;
            }

        }

        return 0;
    }

    static void *ctrl_fun(void *args)
    {
        int data_num;
        char sendbuf[MAXBUF];
        ctrl_args *new_arg = (ctrl_args*) args;
        data_args* my_data_pair = NULL;
        string json_str = new_arg->json_str;

        json recvjson;
        try
        {
            recvjson = json::parse(json_str);
        }
        catch (...)
        {
            cerr << "JSON parse error\n";
            return nullptr;
        }
            
        cout <<"cmd: " << recvjson["cmd"] << endl;
        if(recvjson["cmd"] == "PASV")
        {
            char portnum_str[MAXBUF]; 
            char *result = new char[MAXBUF];
            sockaddr_storage addr;
            socklen_t len = sizeof(sockaddr_storage);

            srand(time(NULL));
            int listen_data_portnum = rand()%40000+1024;
            sprintf(portnum_str,"%d",listen_data_portnum);
            int listen_data_fd = inetlisten((const char*)portnum_str);
            while(listen_data_fd == -1){
                perror("inetlisten");
                listen_data_portnum = rand()%40000+1024;
                sprintf(portnum_str,"%d",listen_data_portnum);
                listen_data_fd = inetlisten((const char*)portnum_str);
            }
            set_nonblocking(listen_data_fd);

            struct epoll_event ev;
            ev.data.fd = listen_data_fd;
            ev.events = EPOLLIN | EPOLLET;
            epoll_ctl(new_arg->epfd,EPOLL_CTL_ADD,listen_data_fd,&ev);
                    
            getsockname(listen_data_fd,(sockaddr*)&addr,&len);
            address_str_portnum(result,MAXBUF,(sockaddr*)&addr,len) == NULL;
            const char delimiter[5] = "().,";
            char *token;
            char **res_token = new char*[10];
            for(int i=0;i<10;i++){
                res_token[i] = new char[10];
            }
            int cnt = 0;
            cout << result << endl;
            token = strtok(result,delimiter);
            while(token != NULL){
                res_token[cnt] = token;
                token = strtok(NULL,delimiter);
                cnt++;
            }
            sprintf(result,"(%s,%s,%s,%s,%d,%d)",res_token[0],res_token[1],res_token[2],res_token[3],atoi(res_token[4])/256,atoi(res_token[4])%256);
            sprintf(sendbuf,"%s %s","227 entering passive mode",result);

            string send_buf = sendbuf; 
            json send_json = {
                {"sort",REFLACT},
                {"request",PASV_CMD},
                {"reflact",send_buf}
            };
            sendjson(send_json,new_arg->fd);

            memset(sendbuf,0,sizeof(sendbuf));
            delete[] res_token;
            free(result);
        }
        else 
        {
            if(recvjson["cmd"] == "LIST"){

                char dirpath[MAXBUF];
                char dirmsg[MAXBUF];
                char tmp_buf[MAXBUF*2];
                char path[LARGESIZE];
                DIR *dirp;

                if(recvjson["run_flag"] == false)
                    getcwd(dirpath,sizeof(dirpath));
                else{
                    string path = recvjson["path"];
                    strcpy(dirpath,path.c_str());
                }

                dirp = opendir(dirpath);
                if(dirp == NULL){
                    perror("opendir");
                    json send_json = {
                        {"sort",REFLACT},
                        {"request",LIST_CMD},
                        {"reflact","wrong dictory"}
                    };
                    cout << "sendjson" << endl;
                    sendjson(send_json,new_arg->fd);
                    return NULL;
                }                    
                struct dirent *file;
                while((file = readdir(dirp)) != NULL){
                    if(strlen(dirmsg) == 0){
                        strcpy(dirmsg,file->d_name);
                        continue;
                    }
                    sprintf(tmp_buf,"%s %s",dirmsg,file->d_name);
                    strncpy(dirmsg,tmp_buf,MAXBUF-1);
                }
                string send_dirmsg = dirmsg; 
                json send_json = {
                    {"sort",REFLACT},
                    {"request",LIST_CMD},
                    {"reflact",send_dirmsg}
                };
                sendjson(send_json,new_arg->fd);
                
                return NULL;
            }

            if(recvjson["cmd"] == "PONG")
            {
                (*new_arg->cfd_to_heart)[new_arg->fd] = time(nullptr);
                cout << "Heartbeat received from cfd " << new_arg->fd << endl;
            }  

            if((recvjson["cmd"] == "RETR") || (recvjson["cmd"] == "STOR"))
            {
                data_num = recvjson["client_num"];
                cout << "pair: [" << data_num << "]" << endl;
                cout << "cmd: [" << recvjson["cmd"] << "]" << endl;
                if(new_arg->data_pairs.count(data_num) > 0)
                    my_data_pair = new_arg->data_pairs[data_num];
                if(my_data_pair == NULL){
                    json send_json = {
                        {"sort",MESSAGE},
                        {"request",START_FILE},
                        {"run_flag",false},
                        {"data_num",data_num},
                        {"reflact","can not find data pair"}
                    };
                    sendjson(send_json,new_arg->fd);
                    return NULL;
                }
                else{
                    json send_json = {
                        {"sort",MESSAGE},
                        {"request",START_FILE},
                        {"run_flag",true},
                        {"data_num",data_num},
                        {"reflact","pair success"}
                    };
                    sendjson(send_json,new_arg->fd);
                }
            }

            if(recvjson["cmd"] == "RETR")
            {
                json send_json;
                char cur_path[LARGESIZE];    
                char open_path[MAXBUF];
                char load_filename[LARGESIZE];
                my_data_pair->retr_flag = true;

                string sender = recvjson["sender"];
                string filepath = recvjson["filepath"];
                string receiver = recvjson["receiver"];
                
                getcwd(cur_path,LARGESIZE);
                replace(filepath.begin(), filepath.end(), '/', '_');
                sprintf(load_filename,"%s_to_%s-%s",sender.c_str(),receiver.c_str(),filepath.c_str());
                sprintf(open_path,"%s/%s/%s",cur_path,"file_tmp",load_filename);
                my_data_pair->retr_filename = open_path;
                cout << "load_filename: " << load_filename << endl;
 
                if(open(my_data_pair->retr_filename.c_str(),O_RDONLY,0644) == -1){
                    send_json = {
                        {"sort",MESSAGE},
                        {"request",RETR_START},
                        {"retr_flag",false},
                        {"data_num",data_num},
                        {"reflact","550 file not found or denied."}  
                    };
                    sendjson(send_json,new_arg->fd);
                return NULL; 
                }
                else{
                    struct stat size_stat;
                    stat(my_data_pair->retr_filename.c_str(),&size_stat);
                    sprintf(sendbuf,"%s %s (%lld %s)","150 Opening BINARY mode data connection "
                            "for",my_data_pair->retr_filename.c_str(),(long long)size_stat.st_size,"bytes");
                    send_json = {
                        {"sort",MESSAGE},
                        {"request",RETR_START},
                        {"retr_flag",true},
                        {"data_num",data_num},
                        {"reflact",sendbuf},
                        {"file_size",size_stat.st_size}  
                    };
                    sendjson(send_json,new_arg->fd);
                }

                new_arg->data_pool->addtask(data_fun,my_data_pair);

                memset(sendbuf,0,sizeof(sendbuf));
                return NULL;
            }
            else if(recvjson["cmd"] == "STOR")
            {                   
                DIR *dirp;
                json send_json;
                char cur_path[LARGESIZE];               
                char creat_name[LARGESIZE];
                char open_path[LARGESIZE*3];
                                
                my_data_pair->stor_flag = true;
                
                string sender = recvjson["sender"];
                string filepath = recvjson["filepath"];
                string receiver = recvjson["receiver"];
                off_t file_size = recvjson["file_size"];

                dirp = opendir("file_tmp");
                if(dirp == nullptr)
                    mkdir("file_tmp",0755);
                getcwd(cur_path,LARGESIZE);

                replace(filepath.begin(), filepath.end(), '/', '_');
                sprintf(creat_name,"%s_to_%s-%s",sender.c_str(),receiver.c_str(),filepath.c_str());
                sprintf(open_path,"%s/%s/%s",cur_path,"file_tmp",creat_name);
                my_data_pair->stor_filename = open_path;
                        
                my_data_pair->stor_filefd = open(open_path,O_CREAT|O_RDWR|O_TRUNC,0644);
                my_data_pair->stor_filesize = file_size;
                if(my_data_pair->stor_filefd == -1)
                {
                    send_json = {
                        {"sort",MESSAGE},
                        {"request",STOR_START},
                        {"stor_flag",false},
                        {"data_num",data_num},
                        {"reflact","550 file not found or denied."}  
                    };
                    sendjson(send_json,new_arg->fd);
                    return NULL;
                }else
                {
                    sprintf(sendbuf,"%s %s","150 Opening BINARY mode data connection for",creat_name);
                    send_json = {
                        {"sort",MESSAGE},
                        {"request",STOR_START},
                        {"stor_flag",true},
                        {"data_num",data_num},
                        {"reflact",sendbuf}  
                    };
                    sendjson(send_json,new_arg->fd);
                }

                return NULL;
            }

        }      
         
        
        return NULL;
    }

    static void *data_fun(void *args)
    {
        char recvbuf[MAXBUF];
        char sendbuf[MAXBUF];
        ssize_t n;
        json send_json;
        data_args *new_arg = (data_args*) args;
        cout << "data_func" << endl;

        if(new_arg->retr_flag)
        {
            int file_fd = 0;
            ssize_t file_size = 0;
            ssize_t send_size = 0;
            off_t off_set = 0;
            struct stat file_stat;

            stat(new_arg->retr_filename.c_str(),&file_stat);
            file_size = file_stat.st_size;
            file_fd = open(new_arg->retr_filename.c_str(),O_RDONLY,0754);

            send_json = {
                {"sort",MESSAGE},
                {"request",END_FILE},
                {"end_flag",true},
                {"data_num",new_arg->data_num},
                {"reflact","226 transfer completed"}
            };
            sendjson(send_json,new_arg->ctrl_pair_fd);

            while (send_size < file_size) 
            {
                size_t chunk = (file_size - send_size) > CHUNK_SIZE 
                               ? CHUNK_SIZE 
                               : (file_size - send_size);

                ssize_t sent = sendfile(new_arg->fd, file_fd, &off_set, chunk);
                
                if (sent < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        usleep(10000);
                        continue;
                    } else {
                        perror("sendfile failed");
                        break;
                    }
                } else if (sent == 0) {
                    break;
                }
                
                send_size += sent;
            }
            
            close(file_fd);
            
            if (send_size != file_size) {
                cout << "incompleted transfer..." << endl;
            }

            return NULL;
            
        }
        else if(new_arg->stor_flag)
        {
            char *file_buf = new char[MAXBUF];
            int file_recvcnt = 0;
            data_args* prev_args = new_arg->data_args_list;
            struct stat file_stat;    

            while(true){
                file_recvcnt = recv(new_arg->fd,file_buf,MAXBUF-1,MSG_DONTWAIT);
                write(new_arg->stor_filefd,file_buf,file_recvcnt);
                memset(file_buf,0,MAXBUF);
                if(file_recvcnt == -1){
                    stat(new_arg->stor_filename.c_str(),&file_stat);
                    if(file_stat.st_size == new_arg->stor_filesize) break;
                    usleep(100);
                }
            }

            if(file_stat.st_size == new_arg->stor_filesize){
                send_json = {
                    {"sort",MESSAGE},
                    {"request",END_FILE},
                    {"end_flag",true},
                    {"data_num",new_arg->data_num},
                    {"reflact","226 transfer completed"}
                };
                sendjson(send_json,new_arg->ctrl_pair_fd);
                cout << "completely receive" << endl; 
            }
            else {
                cout << "actural: " << file_stat.st_size << endl;
                cout << "excepted: " << new_arg->stor_filesize << endl;
                send_json = {
                    {"sort",MESSAGE},
                    {"request",END_FILE},
                    {"end_flag",true},
                    {"data_num",new_arg->data_num},
                    {"reflact","550 Incomplete transfer."}
                };
                sendjson(send_json,new_arg->ctrl_pair_fd);
                cout << "uncompletely receive" << endl;
            }
            
            auto it = new_arg->data_pairs.begin();
            for(;it!= new_arg->data_pairs.end();it++){
                if(it->second->fd == new_arg->fd){
                    new_arg->data_pairs.erase(it);
                    cout << "erasd de_data" << endl;
                    break;
                }
            }

            while(prev_args != nullptr){
                if(prev_args == new_arg){
                    prev_args = prev_args->next;
                    close(new_arg->fd);
                    cout << "close : " << new_arg->fd << endl;
                    free(new_arg);
                    break;
                }
                if(prev_args->next == new_arg){
                    prev_args->next = new_arg->next;
                    close(new_arg->fd);
                    cout << "close : " << new_arg->fd << endl;
                    free(new_arg);
                    break;
                }
                prev_args = prev_args->next;
            }

            return NULL;
        }

        return NULL;
    } 

};

#endif
