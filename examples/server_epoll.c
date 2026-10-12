#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 5000
#define MAX_EVENTS 10
#define BUFF_SIZE 128
#define STORAGE_SIZE 10

char storage[STORAGE_SIZE];
int count = 0; 
int head = 0;  
int tail = 0;  

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(void) {
    struct epoll_event events[MAX_EVENTS];
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(listen_sock);

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    bind(listen_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    listen(listen_sock, SOMAXCONN);

    int epollfd = epoll_create1(0);
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET; 
    ev.data.fd = listen_sock;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev);

    printf("Сервер (epoll ET) запущен на порту %d. Размер буфера: %d\n", PORT, STORAGE_SIZE);

    while (1) {
        int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; i++) {
            
            if (events[i].data.fd == listen_sock) {
                while (1) {
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    int conn_sock = accept(listen_sock, (struct sockaddr *)&cli_addr, &cli_len);
                    
                    if (conn_sock == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break; 
                        perror("accept");
                        break;
                    }
                    
                    printf("[epoll] Подключен новый клиент. FD: %d\n", conn_sock);
                    set_nonblocking(conn_sock); 
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = conn_sock;
                    epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev);
                }
            } 
            else {
                int conn = events[i].data.fd;
                char buffer[BUFF_SIZE];
                char response[BUFF_SIZE];
                
                while (1) {
                    memset(buffer, 0, BUFF_SIZE);
                    memset(response, 0, BUFF_SIZE);

                    int rc = read(conn, buffer, BUFF_SIZE - 1);
                    
                    if (rc == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break; 
                        perror("read error");
                        close(conn);
                        break;
                    } else if (rc == 0) {
                        printf("[epoll] Клиент на FD %d отключился.\n", conn);
                        close(conn); 
                        break;
                    } else {
                        if (buffer[0] == 'P' && strlen(buffer) >= 3) {
                            char item = buffer[2];
                            if (count < STORAGE_SIZE) {
                                storage[tail] = item;
                                tail = (tail + 1) % STORAGE_SIZE;
                                count++;
                                snprintf(response, BUFF_SIZE, "OK");
                                printf("[Сервер] FD:%d положил '%c'. Заполнено: %d/%d\n", conn, item, count, STORAGE_SIZE);
                            } else {
                                snprintf(response, BUFF_SIZE, "FULL");
                                printf("[Сервер] Отказ FD:%d (буфер полон).\n", conn);
                            }
                        } 
                        else if (buffer[0] == 'C') {
                            if (count > 0) {
                                char item = storage[head];
                                head = (head + 1) % STORAGE_SIZE;
                                count--;
                                snprintf(response, BUFF_SIZE, "OK %c", item);
                                printf("[Сервер] FD:%d забрал '%c'. Заполнено: %d/%d\n", conn, item, count, STORAGE_SIZE);
                            } else {
                                snprintf(response, BUFF_SIZE, "EMPTY");
                                printf("[Сервер] Отказ FD:%d (буфер пуст).\n", conn);
                            }
                        }
                        
                        send(conn, response, strlen(response), 0);
                    }
                }
            }
        }
    }
    close(listen_sock);
    return 0;
}
