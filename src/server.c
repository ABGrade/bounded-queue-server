/* Derived from the lr3 producer/consumer socket laboratory. */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define CAPACITY 10
#define MAX_CLIENTS 64
#define MAX_LINE 32
static char storage[CAPACITY];
static size_t head, tail, count, clients;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t stopping;
static int listener=-1;

static void stop_server(int signal_number) {
    (void)signal_number;
    stopping=1;
    if(listener>=0) { close(listener); listener=-1; }
}

static int send_all(int socket_fd, const char* text) {
    size_t left=strlen(text);
    while(left) {
        ssize_t sent=send(socket_fd,text,left,MSG_NOSIGNAL);
        if(sent<0 && errno==EINTR) continue;
        if(sent<=0) return -1;
        left-=(size_t)sent; text+=sent;
    }
    return 0;
}

static void process_line(const char* line, char* reply, size_t size) {
    pthread_mutex_lock(&mutex);
    if(strlen(line)==3 && line[0]=='P' && line[1]==' ' && (unsigned char)line[2]>=32 && (unsigned char)line[2]<127) {
        if(count==CAPACITY) snprintf(reply,size,"FULL\n");
        else { storage[tail]=line[2]; tail=(tail+1)%CAPACITY; ++count; snprintf(reply,size,"OK\n"); }
    } else if(strcmp(line,"C")==0) {
        if(!count) snprintf(reply,size,"EMPTY\n");
        else { char item=storage[head]; head=(head+1)%CAPACITY; --count; snprintf(reply,size,"OK %c\n",item); }
    } else snprintf(reply,size,"ERR\n");
    pthread_mutex_unlock(&mutex);
}

static void* handle_client(void* argument) {
    int socket_fd=*(int*)argument;
    free(argument);
    struct timeval timeout={30,0};
    setsockopt(socket_fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    setsockopt(socket_fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    char line[MAX_LINE],reply[32];
    size_t length=0;
    while(!stopping) {
        char ch;
        ssize_t received=recv(socket_fd,&ch,1,0);
        if(received<0 && errno==EINTR) continue;
        if(received<=0) break;
        if(ch=='\n') {
            if(length && line[length-1]=='\r') --length;
            line[length]='\0';
            process_line(line,reply,sizeof(reply));
            length=0;
            if(send_all(socket_fd,reply)<0) break;
        } else {
            if(ch=='\0' || length>=sizeof(line)-1) { send_all(socket_fd,"ERR\n"); break; }
            line[length++]=ch;
        }
    }
    close(socket_fd);
    pthread_mutex_lock(&mutex); --clients; pthread_mutex_unlock(&mutex);
    return NULL;
}

int main(int argc,char** argv) {
    unsigned long port=5000;
    if(argc>2) { fprintf(stderr,"Usage: %s [port]\n",argv[0]); return 2; }
    if(argc==2) {
        char* end;
        port=strtoul(argv[1],&end,10);
        if(!argv[1][0] || *end || port>65535) { fprintf(stderr,"Invalid port\n"); return 2; }
    }
    struct sigaction action;
    memset(&action,0,sizeof(action)); action.sa_handler=stop_server;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT,&action,NULL);sigaction(SIGTERM,&action,NULL);
    listener=socket(AF_INET,SOCK_STREAM,0);
    if(listener<0) { perror("socket");return 1; }
    int option=1;
    setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&option,sizeof(option));
    struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons((unsigned short)port)};
    address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(listener,(struct sockaddr*)&address,sizeof(address))<0 || listen(listener,32)<0) { perror("listen");close(listener);return 1; }
    socklen_t address_size=sizeof(address);
    getsockname(listener,(struct sockaddr*)&address,&address_size);
    printf("LISTENING 127.0.0.1 %u\n",ntohs(address.sin_port));fflush(stdout);
    while(!stopping) {
        int connection=accept(listener,NULL,NULL);
        if(connection<0) { if(stopping)break; if(errno==EINTR)continue;perror("accept");break; }
        pthread_mutex_lock(&mutex);
        int full=clients>=MAX_CLIENTS;
        if(!full)++clients;
        pthread_mutex_unlock(&mutex);
        if(full) { close(connection);continue; }
        int* argument=malloc(sizeof(int));
        pthread_t thread;
        if(argument) *argument=connection;
        if(!argument || pthread_create(&thread,NULL,handle_client,argument)!=0) {
            free(argument);close(connection);
            pthread_mutex_lock(&mutex);--clients;pthread_mutex_unlock(&mutex);
        } else pthread_detach(thread);
    }
    if(listener>=0)close(listener);
    return 0;
}
