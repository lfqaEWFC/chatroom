#include "/home/mcy-mcy/文档/chatroom/include/inetsockets_tcp.hpp"
#include "/home/mcy-mcy/文档/chatroom/cli/menu.hpp"
#include "/home/mcy-mcy/文档/chatroom/define/define.hpp"
#include "/home/mcy-mcy/文档/chatroom/include/Threadpool.hpp"
#include <sys/epoll.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "User.hpp"
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <filesystem>
#include <errno.h>
#include<dirent.h>
#include<sys/stat.h>

using json = nlohmann::json;

typedef struct file_args
{
    bool retr_flag;
    bool stor_flag;
    bool run_flag;
    bool end_flag;
    bool group_flag = false;
    bool get_num_flag;
    string sender;
    string file_path;
    string receiver;
    int data_num;
    int FTP_data_cfd;
    int FTP_ctrl_cfd;
    int file_fd;
    int chat_server_cfd;
    off_t retr_file_size;
    pthread_cond_t *cond;
    pthread_mutex_t *lock;
    unordered_map<int, file_args *> *datafd_to_file_args;
} file_args;

typedef struct recv_args
{
    int cfd;
    int epfd;
    int FTP_ctrl_cfd;
    pool *file_pool;
    string client_num;
    string *pri_show;
    string *file_path;
    string *username;
    string *fog_username;
    string *fri_username;
    string *group_name;
    string *group_role;
    string *group_show;
    long *group_id;
    bool *id_flag;
    bool *end_flag;
    bool *group_flag;
    bool *group_add_flag;
    bool *end_start_flag;
    bool *fog_que_flag;
    bool *pri_chat_flag;
    bool *get_file_flag;
    bool *chat_name_flag;
    bool *add_friend_req_flag;
    bool *end_chat_flag;
    bool *FTP_data_stor_flag;
    bool *FTP_data_retr_flag;
    vector<string> *add_friend_fri_user;
    vector<string> *add_friend_requests;
    pthread_cond_t *recv_cond;
    pthread_mutex_t *recv_lock;
    unordered_map<int, file_args *> *datafd_to_file_args;
} recv_args;

class client : public menu
{

public:
    client(int in_cfd, int FTP_ctrl_cfd, string client_num) : 
        end_start_flag(false), end_chat_flag(true), end_flag(false), handle_login_flag(true),
        fog_que_flag(false), add_friend_req_flag(false), chat_name_flag(false), pri_chat_flag(false),
        chat_choice(0), start_choice(0),
        cfd(in_cfd), FTP_ctrl_cfd(FTP_ctrl_cfd), client_num(client_num),
        FTP_data_stor_flag(false), FTP_data_retr_flag(false),get_file_flag(false),id_flag(false),
        group_add_flag(false),group_flag(false)
        {
        epfd = epoll_create(EPSIZE);
        ev.data.fd = cfd;
        ev.events = EPOLLET | EPOLLRDHUP | EPOLLIN;
        epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
        ev.data.fd = FTP_ctrl_cfd;
        ev.events = EPOLLET | EPOLLRDHUP | EPOLLIN;
        epoll_ctl(epfd, EPOLL_CTL_ADD, FTP_ctrl_cfd, &ev);

        file_pool = new pool(CLIENT_FILE_NUM);
        args = new recv_args;
        args->cfd = cfd;
        args->epfd = epfd;
        args->id_flag = &id_flag;
        args->FTP_ctrl_cfd = FTP_ctrl_cfd;
        args->file_pool = file_pool;
        args->client_num = client_num;
        args->file_path = &file_path;
        args->end_flag = &end_flag;
        args->username = &username;
        args->pri_show = &pri_show;
        args->group_show = &group_show;
        args->group_flag = &group_flag;
        args->group_id = &group_id;
        args->group_name = &group_name;
        args->group_role = &group_role;
        args->fog_username = &fog_username;
        args->fri_username = &fri_username;
        args->group_add_flag = &group_add_flag;
        args->fog_que_flag = &fog_que_flag;
        args->pri_chat_flag = &pri_chat_flag;
        args->chat_name_flag = &chat_name_flag;
        args->get_file_flag = &get_file_flag;
        args->end_chat_flag = &end_chat_flag;
        args->end_start_flag = &end_start_flag;
        args->add_friend_req_flag = &add_friend_req_flag;
        args->add_friend_requests = &add_friend_requests;
        args->add_friend_fri_user = &add_friend_fri_user;
        args->FTP_data_retr_flag = &FTP_data_retr_flag;
        args->FTP_data_stor_flag = &FTP_data_stor_flag;
        args->datafd_to_file_args = &datafd_to_file_args;

        pthread_cond_init(&recv_cond, nullptr);
        pthread_mutex_init(&recv_lock, nullptr);
        args->recv_cond = &recv_cond;
        args->recv_lock = &recv_lock;
        pthread_create(&recv_pthread, nullptr, recv_thread, args);
    }

    void start(){

        while (!end_flag){

            while (!end_start_flag){

                this->start_show();

                if (!(start_choice = atoi(readline(""))))
                {
                    cout << "请输入数字选项..." << endl;
                    wait_user_continue();
                    system("clear");
                    continue;
                }

                if (!end_start_flag)
                {
                    if (start_choice == LOGIN)
                    {
                        system("clear");
                        json *login = new json;
                        handle_login(login);
                        sendjson(*login, cfd);
                        delete login;
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        if (fog_que_flag && !end_flag)
                        {   
                            char* input;
                            char show[LARGESIZE];
                            char in_ans[64];

                            strcpy(show,"请输入答案: ");
                            input = readline(show);
                            strcpy(in_ans,input);

                            json send_json = {
                                {"request", CHECK_ANS},
                                {"username", fog_username},
                                {"answer", in_ans}
                            };
                            sendjson(send_json, cfd);
                            fog_username.clear();
                            fog_que_flag = false;
                            handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                            free(input);
                            wait_user_continue();
                        }
                        else
                            wait_user_continue();
                    }
                    else if (start_choice == EXIT)
                    {
                        system("clear");
                        cout << "感谢使用聊天室，再见!" << endl;
                        end_start_flag = true;
                        end_flag = true;
                        sleep(1);
                    }
                    else if (start_choice == SIGNIN)
                    {
                        system("clear");
                        json *signin = new json;
                        handle_signin(signin);
                        sendjson(*signin, cfd);
                        delete signin;
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        wait_user_continue();
                    }
                    else if (start_choice == BREAK)
                    {
                        system("clear");
                        json *json_break = new json;
                        ;
                        handle_break(json_break);
                        sendjson(*json_break, cfd);
                        delete json_break;
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        wait_user_continue();
                    }
                    else
                    {
                        cout << "请输入正确的选项..." << endl;
                        wait_user_continue();
                        system("clear");
                        continue;
                    }
                }

                system("clear");
            }

            while (!end_chat_flag){

                if (handle_login_flag && !end_chat_flag)
                {
                    handle_offline_login(cfd, username);
                    handle_success_login(cfd, username);
                    handle_login_flag = false;
                }

                this->chat_show();
                if (!(chat_choice = atoi(readline(""))))
                {
                    cout << "请输入数字选项..." << endl;
                    wait_user_continue();
                    system("clear");
                    continue;
                }

                if (!end_chat_flag)
                {
                    switch (chat_choice)
                    {
                    case 1:
                    {
                        system("clear");
                        json logout = {
                            {"request", LOGOUT},
                            {"username", username}};
                        sendjson(logout, cfd);
                        end_chat_flag = true;
                        end_start_flag = false;
                        handle_login_flag = true;
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        wait_user_continue();
                        break;
                    }
                    case 2:
                    {
                        system("clear");
                        json *chat_name = new json;
                        handle_chat_name(chat_name, username);
                        string fri_user = (*chat_name)["fri_user"];
                        sendjson(*chat_name, cfd);
                        delete chat_name;
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        if (chat_name_flag && !end_flag)
                        {
                            json offline_pri;
                            handle_history_pri(&offline_pri, username);
                            sendjson(offline_pri, cfd);
                            handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                            if (pri_chat_flag && !end_flag)
                            {
                                handle_pri_chat(username, fri_user, cfd, FTP_ctrl_cfd, &end_flag,
                                                &FTP_data_stor_flag, &pri_chat_flag, client_num,
                                                &recv_cond, &recv_lock, &file_path,&fri_username,&pri_show);
                                pri_chat_flag = false;
                            }
                            cout << "=============================================" << endl;
                            wait_user_continue();
                            chat_name_flag = false;
                        }
                        else
                            wait_user_continue();
                        break;
                    }
                    case 3:
                    {
                        system("clear");
                        json *add_friend = new json;
                        handle_add_friend(add_friend, username);
                        sendjson(*add_friend, cfd);
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        wait_user_continue();
                        break;
                    }
                    case 4:
                    {
                        system("clear");
                        json *get_fri_req = new json;
                        handle_get_friend_req(get_fri_req, username);
                        sendjson(*get_fri_req, cfd);
                        delete (get_fri_req);
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        if (add_friend_req_flag && !end_flag)
                        {
                            char* input;
                            bool add_flag = true;
                            vector<string> commit;
                            vector<string> refuse;

                            while (add_flag)
                            {
                                int num;
                                char chk;
                                char show[LARGESIZE];

                                strcpy(show,"请输入需要处理的选项(输入-1退出交互):  ");
                                input = readline(show);
                                num = atoi(input);
                                if ((num < 1 || num > add_friend_requests.size()) && num != -1)
                                {
                                    cout << "编号超出范围，请重新输入。" << endl;
                                    free(input);
                                    continue;
                                }
                                if (num == -1)
                                {
                                    add_flag = false;
                                    free(input);
                                    break;
                                }
                                strcpy(show,"请输入是否同意(y/n): ");
                                input = readline(show);
                                chk = input[0];
                                if (chk == 'y')
                                {
                                    commit.push_back(add_friend_fri_user[num - 1]);
                                }
                                else if (chk == 'n')
                                {
                                    refuse.push_back(add_friend_fri_user[num - 1]);
                                }
                                else
                                {
                                    cout << "请勿输入无关选项..." << endl;
                                }
                            }
                            if (commit.size() == 0 && refuse.size() == 0)
                            {
                                cout << "未处理好友关系..." << endl;
                                sleep(1);
                                system("clear");
                                continue;
                            }
                            json send_json = {
                                {"request", DEAL_FRI_REQ},
                                {"commit", commit},
                                {"refuse", refuse},
                                {"username", username}};
                            sendjson(send_json, cfd);
                            add_friend_req_flag = false;
                            add_friend_fri_user.clear();
                            add_friend_requests.clear();
                            handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                            wait_user_continue();
                        }
                        else
                            wait_user_continue();
                        break;
                    }
                    case 5:{
                        system("clear");
                        handle_show_file(username, cfd,end_flag,&recv_cond,  
                                         &recv_lock,&pri_chat_flag,&group_flag);
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        if(get_file_flag){
                            cout << "=============================================" << endl;
                            handle_retr_file(FTP_ctrl_cfd,end_flag,&recv_lock,&fri_username,
                                            &recv_cond,&FTP_data_retr_flag,&file_path,group_flag);
                            get_file_flag = false;
                            if(group_flag) group_flag = false;
                            if(pri_chat_flag) pri_chat_flag = false;
                        }else wait_user_continue();
                        break;
                    }
                    case 6:
                    {
                        system("clear");
                        handle_black(username, cfd);
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        wait_user_continue();
                        break;
                    }
                    case 7:
                    {
                        system("clear");
                        handle_check_friend(username, cfd);
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        wait_user_continue();
                        break;
                    }
                    case 8:
                    {
                        system("clear");
                        handle_delete_friend(username, cfd);
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        wait_user_continue();
                        break;
                    }
                    case 9:
                    {
                        system("clear");
                        handle_create_group(username,cfd);
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        wait_user_continue();
                        break;
                    }
                    case 10:
                    {
                        system("clear");
                        handle_add_group(username,cfd,end_flag,&id_flag, &recv_cond, &recv_lock);
                        break;
                    }
                    case 11:
                    {
                        system("clear");
                        handle_show_group(cfd,username);
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        handle_group_name(cfd,username);
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        if(group_flag && !end_flag)
                        {
                            handle_history_group(cfd,username,group_id);
                            handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                            handle_group_chat(cfd,username,group_role,group_id,
                                              group_name,end_flag,&group_show,
                                              &recv_cond, &recv_lock, &file_path,
                                              FTP_ctrl_cfd,&FTP_data_stor_flag,&group_flag);
                            group_id = 0;
                            group_role.clear(); 
                            group_name.clear();
                            group_flag = false;
                            wait_user_continue();
                        }
                        else wait_user_continue();
                        break;
                    }
                    case 12:
                    {
                        system("clear");
                        deal_add_group(cfd,username,end_flag,&group_add_flag, &recv_cond, &recv_lock);
                        break;
                    }
                    case 13:
                    {
                        system("clear");
                        handle_show_group(cfd,username);
                        handle_pthread_wait(end_flag, &recv_cond, &recv_lock);
                        wait_user_continue();
                        break;
                    }
                    default:
                    {
                        cout << "请输入正确的选项..." << endl;
                        wait_user_continue();
                        system("clear");
                        continue;
                    }
                    }

                    system("clear");
                }
            }
        }

        return;
    }

private:
    int cfd;
    int epfd;
    int FTP_ctrl_cfd;
    pool *file_pool;
    string file_path;
    string pri_show;
    bool FTP_data_stor_flag;
    bool FTP_data_retr_flag;
    bool id_flag;
    bool end_flag;
    bool group_flag;
    bool group_add_flag;
    bool fog_que_flag;
    bool get_file_flag;
    bool end_chat_flag;
    bool chat_name_flag;
    bool end_start_flag;
    bool pri_chat_flag;
    bool handle_login_flag;
    bool add_friend_req_flag;
    string client_num;
    string username;
    string fri_username;
    string fog_username;
    string group_name;
    string group_role;
    string group_show;
    recv_args *args;
    long group_id;
    int start_choice;
    int chat_choice;
    struct epoll_event ev;
    vector<string> add_friend_fri_user;
    vector<string> add_friend_requests;
    pthread_t recv_pthread;
    pthread_cond_t recv_cond;
    pthread_mutex_t recv_lock;
    unordered_map<int, file_args *> datafd_to_file_args;

    static file_args *find_data_num_pair(int data_num, unordered_map<int, file_args *> *datafd_to_file_args)
    {
        file_args *file_pair = nullptr;
        auto it = datafd_to_file_args->begin();
        for (; it != datafd_to_file_args->end(); it++)
        {
            if (it->second->data_num == data_num)
            {
                file_pair = it->second;
                break;
            }
        }
        return file_pair;
    }

    static string get_filename_safe(const string& path) {
        if (path.empty()) return "";
        namespace fs = filesystem;
        return fs::path(path).filename().string();
    }

    static void *file_pool_func(void *args)
    {
        file_args *new_args = (file_args *)args;

        pthread_cond_wait(new_args->cond, new_args->lock);

        if (new_args->run_flag)
        {
            pthread_cond_wait(new_args->cond, new_args->lock);

            if (new_args->retr_flag)
            {
                DIR* dirp;
                dirent* dirent_name;
                char cur_path[MAXBUF];
                char file_path[2*MAXBUF];
                char file_new_name[LARGESIZE];
                string file_name = new_args->file_path;

                dirp = opendir("download");
                if(dirp == nullptr)
                {
                    mkdir("download",0755);
                    dirp = opendir("download");  // 重新打开
                    if (dirp == nullptr) {
                        perror("opendir failed");
                        return nullptr;
                    }
                }

                getcwd(cur_path,LARGESIZE);
                sprintf(file_path,"%s/%s/%s",cur_path,"download",file_name.c_str()); 
                
                int file_fd = open(file_path,O_CREAT|O_RDWR|O_TRUNC,0644);
                if(file_fd == -1)
                {
                    perror("File open failed");
                    close(new_args->FTP_data_cfd);
                    return nullptr;
                }
                new_args->file_fd = file_fd;
                pthread_cond_wait(new_args->cond, new_args->lock);

                if (new_args->end_flag)
                {
                    char file_buf[MAXBUF];
                    off_t file_recvcnt;
                    off_t file_size = new_args->retr_file_size;
                    struct stat file_stat;

                    while(true)
                    {
                        if((file_recvcnt = recv(new_args->FTP_data_cfd,file_buf,MAXBUF-1,MSG_DONTWAIT)) == -1)
                        {
                            stat(file_path, &file_stat);
                            if(file_size == file_stat.st_size)
                                break;
                            else
                            {
                                usleep(100);
                                continue;
                            }                 
                        }
                        write(new_args->file_fd,file_buf,file_recvcnt);
                        memset(file_buf,0,MAXBUF);
                    }
                    cout << "\r\033[K" << flush;
                    cout << "文件 "+file_name+" 下载成功..." << endl;
                    rl_on_new_line();
                    rl_redisplay();
                    delete (*new_args->datafd_to_file_args)[new_args->FTP_data_cfd];
                    (*new_args->datafd_to_file_args).erase(new_args->FTP_data_cfd);
                    close(new_args->FTP_data_cfd);
                }
                return nullptr;
            }
            if (new_args->stor_flag)
            {   
                string file_name;           
                ssize_t file_size;
                ssize_t send_size;
                off_t off_set = 0;   
                struct stat file_stat;
                string recvbuf[MSGBUF];

                int file_fd = open(new_args->file_path.c_str(), O_RDONLY);
                if (file_fd < 0) 
                {
                    perror("File open failed");
                    close(new_args->FTP_data_cfd);
                    return nullptr;
                }
                
                if (stat(new_args->file_path.c_str(), &file_stat) < 0) 
                {
                    perror("File stat failed");
                    close(file_fd);
                    close(new_args->FTP_data_cfd);
                    return nullptr;
                }
                
                file_size = file_stat.st_size;
                ssize_t total_sent = 0;
                
                while (total_sent < file_size) 
                {
                    size_t chunk = (file_size - total_sent) > CHUNK_SIZE 
                                   ? CHUNK_SIZE 
                                   : (file_size - total_sent);

                    ssize_t sent = sendfile(new_args->FTP_data_cfd, file_fd, &off_set, chunk);
                    
                    if (sent < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            usleep(100);
                            continue;
                        }else
                        {
                            perror("sendfile failed");
                            break;
                        }
                    } else if (sent == 0) {
                        break;
                    }
                    
                    total_sent += sent;
                }
                
                close(file_fd);
                
                if (total_sent != file_size) {
                    cout << "incompleted transfer..." << endl;
                }
                
                pthread_cond_wait(new_args->cond, new_args->lock);

                if (new_args->end_flag)
                {
                    if(!new_args->group_flag){
                        file_name = get_filename_safe(new_args->file_path);
                        json send_json = {
                            {"request",ADD_FILE},
                            {"filename",file_name},
                            {"sender",new_args->sender},
                            {"group_flag",false},
                            {"receiver",new_args->receiver}
                        };
                        sendjson(send_json,new_args->chat_server_cfd);
                    }
                    else{
                        file_name = get_filename_safe(new_args->file_path);
                        json send_json = {
                            {"request",ADD_FILE},
                            {"filename",file_name},
                            {"sender",new_args->sender},
                            {"group_flag",true},
                            {"receiver",new_args->receiver}
                        };
                        sendjson(send_json,new_args->chat_server_cfd);
                    }
                    delete (*new_args->datafd_to_file_args)[new_args->FTP_data_cfd];
                    (*new_args->datafd_to_file_args).erase(new_args->FTP_data_cfd);

                }
                return nullptr;
            }
            if(!new_args->retr_flag && !new_args->stor_flag)
            {
                delete (*new_args->datafd_to_file_args)[new_args->FTP_data_cfd];
                (*new_args->datafd_to_file_args).erase(new_args->FTP_data_cfd);
                close(new_args->FTP_data_cfd);
            }
        }
        
        return nullptr;
    }

    static void *recv_thread(void *args){

        string buffer;
        char recvbuf[MAXBUF] = {0};
        recv_args *new_args = (recv_args *)args;
        struct epoll_event evlist[1];

        while (true && !(*new_args->end_flag)){

            int num = epoll_wait(new_args->epfd, evlist, 1, -1);

            for (int i = 0; i < num; i++)
            {
                int chk_fd = evlist[i].data.fd;
                if (evlist[i].events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP))
                {
                    if ((*new_args->datafd_to_file_args).count(chk_fd) == 0)
                    {
                        cout << "\r\033[K" << flush;
                        cout << "服务器已关闭，当前模块交互结束将退出程序...." << endl;
                        if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                            *new_args->get_file_flag)
                        {
                            rl_on_new_line();
                            rl_redisplay();
                        }
                        else
                        {
                            if(*new_args->pri_chat_flag)
                                cout << *new_args->pri_show << flush;
                            if(*new_args->group_flag)
                                cout << *new_args->group_show << flush;
                        }
                        *new_args->end_flag = true;
                        *new_args->end_chat_flag = true;
                        *new_args->end_start_flag = true;
                        pthread_cond_signal(new_args->recv_cond);
                        return nullptr;
                    }
                    else
                    {
                        close(chk_fd);
                        epoll_ctl(new_args->epfd, EPOLL_CTL_DEL, chk_fd, NULL);
                        continue;
                    }
                }
                else if (evlist[i].events & EPOLLIN)
                {
                    if ((*new_args->datafd_to_file_args).count(chk_fd) > 0)
                    {                       
                        file_args *data_new_args = (*new_args->datafd_to_file_args)[chk_fd];
                        if (data_new_args->get_num_flag)
                        {
                            char in_num[10];
                            recv(evlist[i].data.fd, in_num, sizeof(in_num) - 1, 0);
                            data_new_args->data_num = atoi(in_num);
                            if (data_new_args->stor_flag)
                            {
                                struct stat file_stat;
                                string name = get_filename_safe(new_args->file_path->c_str());
                                stat(new_args->file_path->c_str(),&file_stat);
                                off_t file_size = file_stat.st_size;
                                json send_json = {
                                    {"cmd", "STOR"},
                                    {"filename", name},  
                                    {"client_num", data_new_args->data_num},                                    
                                    {"sender", data_new_args->sender},
                                    {"receiver", data_new_args->receiver},
                                    {"file_size",file_size}
                                };
                                sendjson(send_json,new_args->FTP_ctrl_cfd);
                                new_args->file_pool->addtask(file_pool_func, data_new_args);
                            }
                            else if (data_new_args->retr_flag)
                            {
                                json send_json = {
                                    {"cmd","RETR"},
                                    {"client_num",data_new_args->data_num},
                                    {"filename",new_args->file_path->c_str()},
                                    {"receiver",data_new_args->receiver.c_str()},
                                    {"sender",data_new_args->sender.c_str()}
                                };
                                sendjson(send_json,new_args->FTP_ctrl_cfd);
                                new_args->file_pool->addtask(file_pool_func, data_new_args);
                            }
                            data_new_args->get_num_flag = false;
                        }
                        else
                            new_args->file_pool->addtask(file_pool_func, data_new_args);
                        continue;
                    }

                    ssize_t n;
                    while ((n = recv(evlist[i].data.fd, recvbuf, MAXBUF, MSG_DONTWAIT)) > 0)
                    {
                        buffer.append(recvbuf, n);
                    }
                    if (n == -1)
                    {
                        if (errno != EAGAIN && errno != EWOULDBLOCK)
                        {
                            perror("recv");
                            return nullptr;
                        }
                    }
                    while (buffer.size() >= 4)
                    {

                        uint32_t net_len;
                        memcpy(&net_len, buffer.data(), 4);
                        uint32_t msg_len = ntohl(net_len);
                        if (msg_len == 0)
                        {
                            cerr << "end len: " << msg_len << endl;
                            buffer.clear();
                            break;
                        }
                        if (buffer.size() < 4 + msg_len)
                            break;
                        string json_str = buffer.substr(4, msg_len);
                        buffer.erase(0, 4 + msg_len);

                        json recvjson;
                        
                        try
                        {
                            recvjson = json::parse(json_str);
                        }
                        catch (...)
                        {
                            cerr << "JSON parse error\n";
                            continue;
                        }

                        if (recvjson["sort"] == REFLACT)
                        {
                            if (recvjson["request"] == SIGNIN)
                            {
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if (recvjson["request"] == LOGIN)
                            {
                                if (recvjson["login_flag"])
                                {
                                    *new_args->end_start_flag = true;
                                    *new_args->end_chat_flag = false;
                                    *new_args->username = recvjson["username"];
                                }
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if (recvjson["request"] == FORGET_PASSWORD)
                            {
                                if (recvjson["que_flag"])
                                {
                                    *new_args->fog_que_flag = true;
                                    *new_args->fog_username = recvjson["username"];
                                    string reflact = recvjson["reflact"];
                                    cout << "密保问题: " << reflact << endl;
                                }
                                else
                                {
                                    string reflact = recvjson["reflact"];
                                    cout << reflact << endl;
                                }
                            }
                            else if (recvjson["request"] == CHECK_ANS)
                            {
                                if (recvjson["ans_flag"])
                                {
                                    *new_args->end_start_flag = true;
                                    *new_args->end_chat_flag = false;
                                    *new_args->username = recvjson["username"];
                                }
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if (recvjson["request"] == LOGOUT)
                            {
                                string reflact = recvjson["reflact"];
                                new_args->username->clear(); 
                                cout << reflact << endl;
                            }
                            else if (recvjson["request"] == BREAK)
                            {
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if (recvjson["request"] == ADD_FRIEND)
                            {
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if (recvjson["request"] == GET_FRIEND_REQ)
                            {
                                if (recvjson["do_flag"] == false)
                                {
                                    string reflact = recvjson["reflact"];
                                    cout << reflact << endl;
                                }
                                else
                                {
                                    *new_args->add_friend_req_flag = true;
                                    *new_args->add_friend_fri_user = recvjson["fri_user"];
                                    *new_args->add_friend_requests = recvjson["reflact"];
                                    cout << "你有 " << (*new_args->add_friend_requests).size()
                                         << " 条好友申请：" << endl;
                                    for (size_t i = 0; i < (*new_args->add_friend_requests).size(); ++i)
                                    {
                                        cout << i + 1 << ". " << (*new_args->add_friend_requests)[i] << endl;
                                    }
                                }
                            }
                            else if (recvjson["request"] == DEAL_FRI_REQ)
                            {
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if (recvjson["request"] == CHAT_NAME)
                            {
                                string reflact = recvjson["reflact"];
                                if (recvjson["chat_flag"] == false)
                                    cout << reflact << endl;
                                else
                                {
                                    cout << reflact << endl;
                                    *new_args->fri_username = recvjson["pri_username"];
                                    cout << "=============================================" << endl;
                                    *new_args->chat_name_flag = true;
                                }
                            }
                            else if (recvjson["request"] == GET_HISTORY_PRI)
                            {
                                if (recvjson["ht_flag"] == false)
                                {
                                    string reflact = recvjson["reflact"];
                                    cout << reflact << '\n'
                                         << endl;
                                }
                                else
                                {
                                    json history = recvjson["reflact"];
                                    for (auto &msg : history)
                                    {
                                        string sender = msg["sender"];
                                        string content = msg["content"];
                                        string timestamp = msg["timestamp"];
                                        cout << "[" << timestamp << "] " << sender << ": " << content << endl;
                                    }
                                    cout << '\n';
                                }
                                *new_args->pri_chat_flag = true;
                            }
                            else if (recvjson["request"] == ADD_BLACKLIST)
                            {
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if (recvjson["request"] == REMOVE_BLACKLIST)
                            {
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if (recvjson["request"] == CHECK_FRIEND)
                            {
                                json friends = recvjson["friends"];
                                cout << "好友列表如下: " << endl;
                                for (size_t i = 0; i + 1 < friends.size(); i += 2)
                                {
                                    string name = friends[i];
                                    string status = friends[i + 1];
                                    cout << "好友" << (i + 1) / 2 + 1 << ": " << name << " 状态：" << status << endl;
                                }
                            }
                            else if (recvjson["request"] == DELETE_FRIEND)
                            {
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if (recvjson["request"] == LIST_CMD)
                            {
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if (recvjson["request"] == PASV_CMD)
                            {
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                                if (reflact.size() == 0)
                                {
                                    cout << "227 reflact error" << endl;
                                    continue;
                                }

                                int datafd = handle_pasv(reflact);
                                if (datafd == -1)
                                {
                                    cout << "pasv connect error" << endl;
                                    continue;
                                }
                                int flags = fcntl(datafd, F_GETFL, 0);
                                fcntl(datafd, F_SETFL, flags | O_NONBLOCK);

                                file_args *pool_file_args = new (file_args);
                                pool_file_args->FTP_data_cfd = datafd;
                                pool_file_args->FTP_ctrl_cfd = new_args->FTP_ctrl_cfd;
                                pool_file_args->chat_server_cfd = new_args->cfd;
                                pthread_cond_t *cond = new pthread_cond_t;
                                pthread_mutex_t *lock = new pthread_mutex_t;
                                pthread_cond_init(cond, nullptr);
                                pthread_mutex_init(lock, nullptr);
                                pool_file_args->cond = cond;
                                pool_file_args->lock = lock;
                                pool_file_args->data_num = 0;
                                pool_file_args->get_num_flag = true;
                                pool_file_args->run_flag = false;
                                pool_file_args->end_flag = false;
                                pool_file_args->file_path = *new_args->file_path;
                                pool_file_args->datafd_to_file_args = new_args->datafd_to_file_args;
                                if (*new_args->FTP_data_stor_flag)
                                {
                                    pool_file_args->stor_flag = true;
                                    pool_file_args->retr_flag = false;
                                    *new_args->FTP_data_stor_flag = false;
                                    pool_file_args->sender = *new_args->username;
                                    if(*new_args->pri_chat_flag)
                                        pool_file_args->receiver = *new_args->fri_username;
                                    if(*new_args->group_flag){
                                        pool_file_args->receiver = to_string(*new_args->group_id);
                                        pool_file_args->group_flag = true;
                                    }
                                }
                                else if (*new_args->FTP_data_retr_flag) 
                                {
                                    pool_file_args->stor_flag = false;
                                    pool_file_args->retr_flag = true;
                                    *new_args->FTP_data_retr_flag = false;
                                    if(*new_args->pri_chat_flag){
                                        pool_file_args->receiver = *new_args->username;
                                        pool_file_args->sender = *new_args->fri_username;
                                    }
                                    if(*new_args->group_flag){
                                        pool_file_args->sender = *new_args->fri_username;
                                        pool_file_args->receiver = to_string(*new_args->group_id);
                                        pool_file_args->group_flag = true;
                                    }
                                }
                                else
                                {
                                    cout << "FTP_data_flag error" << endl;
                                    close(datafd);
                                    continue;
                                }
                                (*new_args->datafd_to_file_args)[datafd] = pool_file_args;
                                struct epoll_event ev;
                                ev.data.fd = datafd;
                                ev.events = EPOLLIN | EPOLLONESHOT;
                                epoll_ctl(new_args->epfd, EPOLL_CTL_ADD, datafd, &ev);
                                send(datafd, new_args->client_num.c_str(), new_args->client_num.size() + 1, 0);
                            }
                            else if(recvjson["request"] == SHOW_FILE)
                            {
                                bool get_flag = recvjson["get_flag"];
                                bool group_flag = recvjson["group_flag"];
                                if(get_flag)
                                {
                                    if(!group_flag){
                                        json filenames = recvjson["reflact"];
                                        cout << "可接收文件如下: " << endl;
                                        for (size_t i = 0; i <filenames.size(); i++)
                                        {
                                            string name = filenames[i];
                                            cout << "文件名称: " << name << endl;
                                        }
                                        *new_args->fri_username = recvjson["file_user"];
                                    }
                                    else
                                    {
                                        long gid = recvjson["group_id"];
                                        json array = recvjson["reflact"];
                                        cout << "文件信息如下： " << endl;
                                        for (size_t i = 0;i<array.size();i++)
                                        {
                                            json msg = array[i];
                                            string sender = msg["sender"];
                                            string filename = msg["filename"];
                                            cout << "发送者：" << sender;
                                            cout << "   文件名称：" << filename << endl;
                                        }
                                        *new_args->group_id = gid;
                                    }
                                    *new_args->get_file_flag = true;
                                }
                                else {
                                    string reflact = recvjson["reflact"];
                                    cout << reflact << endl;
                                    new_args->fri_username->clear();
                                }                              
                            }
                            else if(recvjson["request"] == CREATE_GROUP)
                            {
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if(recvjson["request"] == SEL_GROUP){
                                *new_args->id_flag = recvjson["id_flag"];
                                cout << "=============================================" << endl;
                                if(*new_args->id_flag)
                                {
                                    json sel_groups = recvjson["sel_groups"];
                                    for(int i=0;i<sel_groups.size();i++)
                                    {
                                        int group_id = sel_groups[i]["group_id"];
                                        string owner_name = sel_groups[i]["owner_name"];
                                        cout << "群聊id: " << group_id ;
                                        cout <<  "    群主: " << owner_name << endl;
                                    }
                                    cout << "=============================================" << endl;
                                }
                                else
                                {
                                    string reflact = recvjson["reflact"];
                                    cout << reflact << endl;
                                }
                            }
                            else if(recvjson["request"] == ADD_GROUP)
                            {
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if(recvjson["request"] == DEAL_ADDGROUP)
                            {
                                if(recvjson["group_add_flag"])
                                {
                                    json elements = recvjson["reflact"];
                                    for(int i=0;i<elements.size();i++)
                                    {
                                        int group_id = elements[i]["gid"];
                                        string owner_name = elements[i]["username"];
                                        cout << "群聊id: " << group_id ;
                                        cout <<  "    请求用户: " << owner_name << endl;
                                    }
                                    *new_args->group_add_flag = true;
                                }
                                else
                                {
                                    string reflact = recvjson["reflact"];
                                    cout << reflact << endl;
                                }
                                cout << "=============================================" << endl;
                            }
                            else if(recvjson["request"] == COMMIT_ADD){
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if(recvjson["request"] == SHOW_GROUP){
                                if(recvjson["show_flag"])
                                {
                                    cout << "=============================================" << endl;
                                    cout << "以下是群聊列表: " << endl;
                                    json elements = recvjson["elements"];
                                    for(int i=0;i<elements.size();i++)
                                    {
                                        json msg = elements[i];
                                        long gid = msg["gid"];
                                        string role = msg["role"];
                                        string group_name = msg["group_name"];
                                        string owner_name = msg["owner_name"];
                                        cout << "群聊id: " << gid ;
                                        cout << "   群聊名称: " << group_name;
                                        cout << "   群主: " << owner_name;
                                        cout << "   角色: " << role << endl;;
                                    }
                                }
                                else
                                {
                                    string reflact = recvjson["reflact"];
                                    cout << reflact << endl;
                                }
                                cout << "=============================================" << endl;
                            }
                            else if(recvjson["request"] == GROUP_NAME){
                                if(recvjson["group_flag"])
                                {
                                    *new_args->group_role = recvjson["role"];
                                    *new_args->group_id = recvjson["gid"];
                                    *new_args->group_name = recvjson["group_name"];
                                    *new_args->group_flag = true;
                                }
                                else
                                {
                                    string reflact = recvjson["reflact"];
                                    cout << reflact << endl;
                                }
                            }
                            else if(recvjson["request"] == GROUP_HISTORY){
                                if(recvjson["his_flag"])
                                {
                                    json history = recvjson["reflact"];
                                    for (int i = history.size() - 1; i >= 0; --i)
                                    {
                                        string sender = history[i]["sender"];
                                        string content = history[i]["content"];
                                        string timestamp = history[i]["timestamp"];
                                        cout << "[" << timestamp << "] " << sender << ": " << content << endl;
                                    }
                                    cout << '\n';
                                }
                                else
                                {
                                    string reflact = recvjson["reflact"];
                                    cout << reflact << endl;
                                }
                            }
                            else if(recvjson["request"] == GROUP_END){
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if(recvjson["request"] == SHOW_MEMBER){
                                json members = json::array();
                                members = recvjson["members"];
                                cout << "=============================================" << endl;
                                for(int i=0; i<members.size();i++)
                                {
                                    json msg = members[i];
                                    string username = msg["username"];
                                    string role = msg["role"];
                                    cout << "成员：" << username;
                                    cout << "   权限：" << role << endl;
                                }
                                cout << "=============================================" << endl;
                            }
                            else if(recvjson["request"] == ADD_ADMIN){
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if(recvjson["request"] == SHOW_FRIEND){
                                json members = json::array();
                                members = recvjson["members"];
                                cout << "=============================================" << endl;
                                for(int i=0; i<members.size();i++)
                                {
                                    string username = members[i];
                                    cout << "好友：" << username << endl;
                                }
                                cout << "=============================================" << endl;
                            }
                            else if(recvjson["request"] == ADD_GROUP_MEM){
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if(recvjson["request"] == BREAK_GROUP){
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if(recvjson["request"] == KILL_GROUP_USER){
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else if(recvjson["request"] == DELETE_GROUP){
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                            }
                            else{
                                cout << "错误的request,服务端可能出错..." <<endl;
                                continue;
                            }
                            pthread_cond_signal(new_args->recv_cond);
                            continue;
                        }
                        else if (recvjson["sort"] == MESSAGE)
                        {
                            if (recvjson["request"] == ASK_ADD_FRIEND)
                            {
                                string message  = recvjson["message"];
                                cout << "\r\033[K" << flush;
                                cout << message << endl;
                                if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                    *new_args->get_file_flag)
                                {
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                else
                                {
                                    if(*new_args->pri_chat_flag)
                                        cout << *new_args->pri_show << flush;
                                    if(*new_args->group_flag)
                                        cout << *new_args->group_show << flush;
                                }
                            }
                            else if (recvjson["request"] == ADD_BLACKLIST)
                            {
                                string reflact;
                                *new_args->pri_chat_flag = false;
                                cout << "\r\033[K" << flush;
                                cout << reflact << endl;
                                if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                    *new_args->get_file_flag)
                                {
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                else
                                {
                                    if(*new_args->pri_chat_flag)
                                        cout << *new_args->pri_show << flush;
                                    if(*new_args->group_flag)
                                        cout << *new_args->group_show << flush;
                                }
                            }
                            else if (recvjson["request"] == GET_OFFLINE_MSG)
                            {
                                json elements = json::array();
                                elements = recvjson["elements"];
                                int count = elements.size();
                                if (count > 0)
                                {
                                    cout << "以下是新消息: " << endl;
                                    for (int i = 0; i < count; i++)
                                    {
                                        string message = elements[i];
                                        cout << message << endl;
                                    }
                                }
                            }
                            else if (recvjson["request"] == PEER_CHAT)
                            {
                                string sender = recvjson["sender"];
                                string receiver = recvjson["receiver"];
                                string message = recvjson["message"];
                                cout << "\r\033[K" << flush;
                                cout << "[" << sender << "->" << receiver << "]: ";
                                cout << message << endl;
                                cout << *new_args->pri_show << flush;
                            }
                            else if (recvjson["request"] == NON_PEER_CHAT)
                            {
                                string message = recvjson["message"];
                                cout << "\r\033[K" << flush;
                                cout << message << endl;
                                if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                    *new_args->get_file_flag)
                                {
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                else
                                {
                                    if(*new_args->pri_chat_flag)
                                        cout << *new_args->pri_show << flush;
                                    if(*new_args->group_flag)
                                        cout << *new_args->group_show << flush;
                                }
                            }
                            else if (recvjson["request"] == START_FILE)
                            {
                                bool run_flag = recvjson["run_flag"];
                                int data_num = recvjson["data_num"];
                                file_args *file_pair = NULL;
                                file_pair = find_data_num_pair(data_num, new_args->datafd_to_file_args);
                                if (file_pair == nullptr)
                                {
                                    cout << "\r\033[K" << flush;
                                    cout << "client can not find file_pair" << endl;
                                    rl_on_new_line();
                                    if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                        *new_args->get_file_flag)
                                    {
                                        rl_on_new_line();
                                        rl_redisplay();
                                    }
                                    else
                                    {
                                        if(*new_args->pri_chat_flag)
                                            cout << *new_args->pri_show << flush;
                                        if(*new_args->group_flag)
                                            cout << *new_args->group_show << flush;
                                    }
                                    *new_args->end_flag = true;
                                    *new_args->end_start_flag = true;
                                    *new_args->end_chat_flag = true;
                                    continue;
                                }
                                if (run_flag)
                                    file_pair->run_flag = true;
                                else{
                                    cout << "\r\033[K" << flush;
                                    string reflact = recvjson["reflact"];
                                    cout << reflact << endl;
                                    if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                        *new_args->get_file_flag)
                                    {
                                        rl_on_new_line();
                                        rl_redisplay();
                                    }
                                    else
                                    {
                                        if(*new_args->pri_chat_flag)
                                            cout << *new_args->pri_show << flush;
                                        if(*new_args->group_flag)
                                            cout << *new_args->group_show << flush;
                                    }
                                    close(file_pair->FTP_data_cfd);
                                }
                                pthread_cond_signal(file_pair->cond);
                            }
                            else if (recvjson["request"] == RETR_START)
                            {
                                bool retr_flag = recvjson["retr_flag"];
                                int data_num = recvjson["data_num"];
                                cout << "\r\033[K" << flush;
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                                if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                    *new_args->get_file_flag)
                                {
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                else
                                {
                                    if(*new_args->pri_chat_flag)
                                        cout << *new_args->pri_show << flush;
                                    if(*new_args->group_flag)
                                        cout << *new_args->group_show << flush;
                                }
                                file_args *file_pair = NULL;
                                file_pair = find_data_num_pair(data_num, new_args->datafd_to_file_args);
                                if (file_pair == nullptr)
                                {
                                    cout << "\r\033[K" << flush;
                                    cout << "client can not find file_pair" << endl;
                                    if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                        *new_args->get_file_flag)
                                    {
                                        rl_on_new_line();
                                        rl_redisplay();
                                    }
                                    else
                                    {
                                        if(*new_args->pri_chat_flag)
                                            cout << *new_args->pri_show << flush;
                                        if(*new_args->group_flag)
                                            cout << *new_args->group_show << flush;
                                    }
                                    *new_args->end_flag = true;
                                    *new_args->end_start_flag = true;
                                    *new_args->end_chat_flag = true;
                                    continue;
                                }
                                if (!retr_flag){
                                    file_pair->retr_flag = false;
                                    close(file_pair->FTP_data_cfd);
                                }
                                else{
                                    file_pair->retr_flag = true;
                                    file_pair->retr_file_size = recvjson["file_size"];
                                }
                                pthread_cond_signal(file_pair->cond);
                            }
                            else if(recvjson["request"] == ADD_FILE){
                                string message = recvjson["message"];
                                cout << "\r\033[K" << flush;
                                cout << message << endl;
                                if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                    *new_args->get_file_flag)
                                {
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                else
                                {
                                    if(*new_args->pri_chat_flag)
                                        cout << *new_args->pri_show << flush;
                                    if(*new_args->group_flag)
                                        cout << *new_args->group_show << flush;
                                }
                            }
                            else if (recvjson["request"] == STOR_START)
                            {
                                bool stor_flag = recvjson["stor_flag"];
                                int data_num = recvjson["data_num"];
                                cout << "\r\033[K" << flush;
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                                if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                    *new_args->get_file_flag)
                                {
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                else
                                {
                                    if(*new_args->pri_chat_flag)
                                        cout << *new_args->pri_show << flush;
                                    if(*new_args->group_flag)
                                        cout << *new_args->group_show << flush;
                                }
                                file_args *file_pair = NULL;
                                file_pair = find_data_num_pair(data_num, new_args->datafd_to_file_args);
                                if (file_pair == nullptr)
                                {
                                    cout << "\r\033[K" << flush;
                                    cout << "client can not find file_pair" << endl;
                                    if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                        *new_args->get_file_flag)
                                    {
                                        rl_on_new_line();
                                        rl_redisplay();
                                    }
                                    else
                                    {
                                        if(*new_args->pri_chat_flag)
                                            cout << *new_args->pri_show << flush;
                                        if(*new_args->group_flag)
                                            cout << *new_args->group_show << flush;
                                    }
                                    *new_args->end_flag = true;
                                    *new_args->end_start_flag = true;
                                    *new_args->end_chat_flag = true;
                                    continue;
                                }
                                if (!stor_flag){
                                    file_pair->stor_flag = false;
                                    close(file_pair->FTP_data_cfd);
                                }
                                else
                                    file_pair->stor_flag = true;    
                                pthread_cond_signal(file_pair->cond);
                            }
                            else if (recvjson["request"] == END_FILE)
                            {
                                bool end_flag = recvjson["end_flag"];
                                int data_num = recvjson["data_num"];
                                cout << "\r\033[K" << flush;
                                string reflact = recvjson["reflact"];
                                cout << reflact << endl;
                                if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                    *new_args->get_file_flag)
                                {
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                else
                                {
                                    if(*new_args->pri_chat_flag)
                                        cout << *new_args->pri_show << flush;
                                    if(*new_args->group_flag)
                                        cout << *new_args->group_show << flush;
                                }
                                file_args *file_pair = NULL;
                                file_pair = find_data_num_pair(data_num, new_args->datafd_to_file_args);
                                if (file_pair == nullptr)
                                {
                                    cout << "\r\033[K" << flush;
                                    cout << "client can not find file_pair" << endl;
                                    if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                        *new_args->get_file_flag)
                                    {
                                        rl_on_new_line();
                                        rl_redisplay();
                                    }
                                    else
                                    {
                                        if(*new_args->pri_chat_flag)
                                            cout << *new_args->pri_show << flush;
                                        if(*new_args->group_flag)
                                            cout << *new_args->group_show << flush;
                                    }
                                    *new_args->end_flag = true;
                                    *new_args->end_start_flag = true;
                                    *new_args->end_chat_flag = true;
                                    continue;
                                }
                                if (end_flag)
                                    file_pair->end_flag = true;
                                usleep(10000);
                                pthread_cond_signal(file_pair->cond);
                            }
                            else if(recvjson["request"] == ADD_GROUP){
                                string message = recvjson["message"];
                                cout << "\r\033[K" << flush;
                                cout << message << endl;
                                if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                    *new_args->get_file_flag)
                                {
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                else
                                {
                                    if(*new_args->pri_chat_flag)
                                        cout << *new_args->pri_show << flush;
                                    if(*new_args->group_flag)
                                        cout << *new_args->group_show << flush;
                                }
                            }
                            else if(recvjson["request"] == NOT_PEER_GROUP)
                            {   
                                string message = recvjson["message"];
                                cout << "\r\033[K" << flush;
                                cout << message << endl;
                                if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                    *new_args->get_file_flag)
                                {
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                else
                                {
                                    if(*new_args->pri_chat_flag)
                                        cout << *new_args->pri_show << flush;
                                    if(*new_args->group_flag)
                                        cout << *new_args->group_show << flush;
                                }
                            }
                            else if(recvjson["request"] == PEER_GROUP)
                            {
                                string sender = recvjson["sender"];
                                string message = recvjson["message"];
                                string show;
                                cout << "\r\033[K" << flush;
                                cout << "[ "+*new_args->group_name+" ] "+sender+": ";
                                cout << message << endl;
                                cout << *new_args->group_show << flush;
                            }
                            else if(recvjson["request"] == BREAK_GROUP)
                            {
                                string message = recvjson["message"];
                                bool outflag = recvjson["outflag"];
                                cout << "outflag: " << outflag << endl;

                                cout << "\r\033[K" << flush;
                                cout << message << endl;
                                if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                    *new_args->get_file_flag)
                                {
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                else
                                {
                                    if(*new_args->pri_chat_flag)
                                        cout << *new_args->pri_show << flush;
                                    if(*new_args->group_flag)
                                        cout << *new_args->group_show << flush;
                                }
                                if(outflag) *new_args->group_flag = false;
                            }
                            else if(recvjson["request"] == KILL_GROUP_USER)
                            {
                                string message = recvjson["message"];
                                bool outflag = recvjson["outflag"];
                                cout << "outflag: " << outflag << endl;
                                
                                cout << "\r\033[K" << flush;
                                cout << message << endl;
                                if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                    *new_args->get_file_flag)
                                {
                                    rl_on_new_line();
                                    rl_redisplay();
                                }
                                else
                                {
                                    if(*new_args->pri_chat_flag)
                                        cout << *new_args->pri_show << flush;
                                    if(*new_args->group_flag)
                                        cout << *new_args->group_show << flush;
                                }
                                if(outflag) *new_args->group_flag = false;
                            }
                            else 
                                cout << "错误的request,服务端可能出错..." << endl;
                            continue;
                        }
                        else if (recvjson["sort"] == ERROR)
                        {
                            cout << "\r\033[K" << flush;
                            cout << "发生错误 : " << recvjson["reflact"] << endl;
                            if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                *new_args->get_file_flag)
                            {
                                rl_on_new_line();
                                rl_redisplay();
                            }
                            else
                            {
                                if(*new_args->pri_chat_flag)
                                    cout << *new_args->pri_show << flush;
                                if(*new_args->group_flag)
                                    cout << *new_args->group_show << flush;
                            }
                            *new_args->end_flag = true;
                            *new_args->end_start_flag = true;
                            *new_args->end_chat_flag = true;
                            pthread_cond_signal(new_args->recv_cond);
                            return nullptr;
                        }
                        else
                        {
                            cout << "\r\033[K" << flush;
                            cout << "接受信息类型错误: " << recvjson["sort"] << endl;
                            if((!(*new_args->pri_chat_flag) && !(*new_args->group_flag)) ||
                                *new_args->get_file_flag)
                            {
                                rl_on_new_line();
                                rl_redisplay();
                            }
                            else
                            {
                                if(*new_args->pri_chat_flag)
                                    cout << *new_args->pri_show << flush;
                                if(*new_args->group_flag)
                                    cout << *new_args->group_show << flush;
                            }
                            *new_args->end_flag = true;
                            *new_args->end_start_flag = true;
                            *new_args->end_chat_flag = true;
                            pthread_cond_signal(new_args->recv_cond);
                            return nullptr;
                        }
                    }
                }
            }

            memset(recvbuf, 0, MAXBUF);
        }

        return nullptr;
    }
};
