#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

#define PORT 5000
#define BUFF_SIZE 128

int main(int argc, char *argv[])
{
    if (argc != 2 || (strcmp(argv[1], "p") != 0 && strcmp(argv[1], "c") != 0))
    {
        fprintf(stderr, "Использование: %s <p|c>\n", argv[0]);
        fprintf(stderr, "  p - Производитель (Producer)\n");
        fprintf(stderr, "  c - Потребитель (Consumer)\n");
        exit(EXIT_FAILURE);
    }

    int is_producer = (strcmp(argv[1], "p") == 0);
    srand(time(NULL) ^ getpid());

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("Ошибка создания сокета");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT)};
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("Ошибка подключения к серверу");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("[PID %d] Подключен. Режим: %s\n", getpid(), is_producer ? "ПРОИЗВОДИТЕЛЬ" : "ПОТРЕБИТЕЛЬ");

    char request[BUFF_SIZE];
    char response[BUFF_SIZE];

    while (1)
    {
        memset(request, 0, BUFF_SIZE);
        memset(response, 0, BUFF_SIZE);

        char item_to_produce = '\0';

        is_producer = rand() % 2;

        if (is_producer)
        {
            item_to_produce = 'A' + (rand() % 26);
            snprintf(request, BUFF_SIZE, "P %c", item_to_produce);
        }
        else
        {
            snprintf(request, BUFF_SIZE, "C");
        }

        if (send(sock, request, strlen(request), 0) == -1)
        {
            perror("Ошибка отправки (сервер отключился?)");
            break;
        }

        int rc = read(sock, response, BUFF_SIZE - 1);
        if (rc <= 0)
        {
            printf("[PID %d] Сервер разорвал соединение.\n", getpid());
            break;
        }

        if (is_producer)
        {
            if (strncmp(response, "OK", 2) == 0)
            {
                printf("[Производитель] Создал и положил на склад: '%c'\n", item_to_produce);
            }
            else if (strncmp(response, "FULL", 4) == 0)
            {
                printf("[Производитель] Не смог положить '%c' - склад ПЕРЕПОЛНЕН!\n", item_to_produce);
            }
            sleep(3);
        }
        else
        {
            if (strncmp(response, "OK", 2) == 0 && strlen(response) >= 4)
            {
                char consumed_item = response[3];
                printf("[Потребитель] Забрал со склада и использовал: '%c'\n", consumed_item);
            }
            else if (strncmp(response, "EMPTY", 5) == 0)
            {
                printf("[Потребитель] Хотел забрать товар, но склад ПУСТ!\n");
            }
            sleep(2);
        }
    }

    close(sock);
    return 0;
}
