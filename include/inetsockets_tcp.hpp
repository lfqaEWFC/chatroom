#include<netdb.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<iostream>
#include<string.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<fcntl.h>

using namespace std;

#define BACKLOG 50
#define MAXBUF 4096

void get_local_ip(char* ipstr);
int inetlisten(const char *portnum);
int inetconnect(char *service,const char *portnum);
char* address_str_portnum(char *result,ssize_t max_resultlen,sockaddr *addr,socklen_t addrlen);
int set_nonblocking(int fd);
