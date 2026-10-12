#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 5000
#define BUFF_SIZE 128
#define STORAGE_SIZE 10


char storage[STORAGE_SIZE];
int count = 0; 
int head = 0;  
int tail = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg) {
    int conn_sock = *(int *)arg;
    free(arg); 
    
    char buffer[BUFF_SIZE];
    char response[BUFF_SIZE];

    printf("[Поток %lu] Клиент подключился. Дескриптор: %d\n", pthread_self(), conn_sock);

    while (1) {
        memset(buffer, 0, BUFF_SIZE);
        memset(response, 0, BUFF_SIZE);

        int rc = read(conn_sock, buffer, BUFF_SIZE - 1);
        if (rc <= 0) {
            printf("[Поток %lu] Клиент отключился.\n", pthread_self());
            break;
        }

        pthread_mutex_lock(&mutex); 
        
        
        if (buffer[0] == 'P' && strlen(buffer) >= 3) {
            char item = buffer[2];
            if (count < STORAGE_SIZE) {
                storage[tail] = item;
                tail = (tail + 1) % STORAGE_SIZE;
                count++;
                snprintf(response, BUFF_SIZE, "OK");
                printf("[Сервер] Добавлен символ '%c'. Заполнено: %d/%d\n", item, count, STORAGE_SIZE);
            } else {
                snprintf(response, BUFF_SIZE, "FULL");
                printf("[Сервер] Отказ производителю (буфер полон).\n");
            }
        } 
        
        else if (buffer[0] == 'C') {
            if (count > 0) {
                char item = storage[head];
                head = (head + 1) % STORAGE_SIZE;
                count--;
                snprintf(response, BUFF_SIZE, "OK %c", item);
                printf("[Сервер] Выдан символ '%c'. Заполнено: %d/%d\n", item, count, STORAGE_SIZE);
            } else {
                snprintf(response, BUFF_SIZE, "EMPTY");
                printf("[Сервер] Отказ потребителю (буфер пуст).\n");
            }
        }
        
        pthread_mutex_unlock(&mutex); 
        send(conn_sock, response, strlen(response), 0);
    }
    
    close(conn_sock);
    return NULL;
}

int main(void) {
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    bind(listen_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    listen(listen_sock, 10);

    printf("Сервер запущен на порту %d. Размер буфера: %d\n", PORT, STORAGE_SIZE);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *conn_sock = malloc(sizeof(int));
        *conn_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);

        if (*conn_sock != -1) {
            pthread_t tid;
            pthread_create(&tid, NULL, handle_client, conn_sock);
            pthread_detach(tid); 
        } else {
            free(conn_sock);
        }
    }
    close(listen_sock);
    return 0;
}
