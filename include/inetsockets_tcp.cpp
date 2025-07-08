#include "inetsockets_tcp.hpp"

//this function can get local ip
void get_local_ip(char* ipstr) {

    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in google_dns{};
    google_dns.sin_family = AF_INET;
    google_dns.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &google_dns.sin_addr);
    connect(fd, (sockaddr*)&google_dns, sizeof(google_dns));

    sockaddr_in local_addr{};
    socklen_t len = sizeof(local_addr);
    getsockname(fd, (sockaddr*)&local_addr, &len);

    inet_ntop(AF_INET, &local_addr.sin_addr, ipstr, INET_ADDRSTRLEN);
    
    close(fd);
}

//If successful, inetlisten sets a socket to listen mode and returns the listening file descriptor. 
//If inetlisten fails, it returns -1.
int inetlisten(const char *portnum){

    struct addrinfo *hints;
    struct addrinfo *result,*rp;
    char *local_ip = new char[MAXBUF];
    int lfd = 0,oprval = 1;

    hints = new struct addrinfo;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_family = AF_UNSPEC;
    hints->ai_flags = AI_NUMERICSERV;
    hints->ai_canonname = NULL;
    hints->ai_addr = NULL;
    hints->ai_next = NULL;

    get_local_ip(local_ip);
    if(getaddrinfo(local_ip,portnum,hints,&result) != 0){
        free(hints);
        return -1;
    }
    for(rp = result;rp!=NULL;rp = rp->ai_next){
        lfd = socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol);
        if(lfd == -1)
            continue;
        if(setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&oprval,sizeof(oprval)) == -1){
            close(lfd);
            free(hints);
            freeaddrinfo(result);
            return -1;
        }
        if(bind(lfd,rp->ai_addr,rp->ai_addrlen) == 0)
            break;
        else{
            if(rp->ai_next == NULL){
                close(lfd);
                free(hints);
                freeaddrinfo(result);
                return -1;
            }
        }
        close(lfd);
    }
    free(hints);
    freeaddrinfo(result);

    if(listen(lfd,BACKLOG) == -1)
        return -1; 

    return lfd;
}

//If successful, inetconnect sets a socket to connect service and returns the connecting file descriptor. 
//If innetconnect fails, it returns -1.
int inetconnect(char *service,const char *portnum){

    struct addrinfo *hints;
    struct addrinfo *result,*rp;
    int cfd = 0,chk = 0;

    hints = new struct addrinfo;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_family = AF_UNSPEC;
    hints->ai_flags = AI_NUMERICSERV;
    hints->ai_canonname = NULL;
    hints->ai_addr = NULL;
    hints->ai_next = NULL;

    if(getaddrinfo(service,portnum,hints,&result) != 0){
        free(hints);
        return -1;
    }
    for(rp = result;rp!=NULL;rp=rp->ai_next){
        cfd = socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol);
        if(cfd == -1)
            continue;
        if(connect(cfd,rp->ai_addr,rp->ai_addrlen) != -1){
            chk = 1;
            break;
        }
        close(cfd);
    }
    free(hints);
    freeaddrinfo(result);

    if(chk == 0)
        return -1;

    return cfd;
}

//this function can transform address
//if function fails , it returns nullptr
//if successful ,it returns portnum
char* address_str_portnum(char *result,ssize_t max_resultlen,sockaddr *addr,socklen_t addrlen){

    char host[MAXBUF],*service;
    service = new char[MAXBUF];
    if(getnameinfo(addr,addrlen,host,MAXBUF,service,MAXBUF,NI_NUMERICHOST) == 0){
        snprintf(result,max_resultlen,"(%s,%s)",host,service);
    }
    else{
        perror("getnameinfo");
        return NULL;
    }
    return service;
}

//this function can convert the socket descriptor to non-blocking state
//if successful , it returns 0
//if fails ,it returns -1
int set_nonblocking(int fd) {

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

bool sendjson(json sendjson,int cfd){

    string json_str = sendjson.dump();
    const char* data = json_str.c_str(); 
    uint32_t data_len = json_str.size();

    uint32_t net_len = htonl(data_len);

    string send_buf;
    send_buf.append(reinterpret_cast<char*>(&net_len), sizeof(net_len));
    send_buf.append(json_str);

    size_t total_sent = 0;
    size_t to_send = send_buf.size();
     
    while (total_sent < to_send) {
        ssize_t sent = send(cfd, send_buf.c_str() + total_sent, to_send - total_sent, 0);
        if (sent == -1) {
            perror("send");
            return false;
        }
        total_sent += sent;
    }

    return true;
}

void wait_user_continue(){
    cout << "请按回车继续..." << endl;
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); 
    cin.get();
}
