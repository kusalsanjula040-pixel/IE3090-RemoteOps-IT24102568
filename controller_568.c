#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 9410
#define BUFFER_SIZE 1024

void *handle_client(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];

    printf("Client thread started.\n");

    memset(buffer, 0, BUFFER_SIZE);

    int bytes_received = recv(
        client_fd,
        buffer,
        BUFFER_SIZE - 1,
        0
    );

    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';

        printf("Client says: %s", buffer);

        char response[] = "Hello Client\n";

        send(
            client_fd,
            response,
            strlen(response),
            0
        );
    }

    close(client_fd);

    printf("Client thread finished.\n");

    return NULL;
}

int main()
{
    int server_fd;

    struct sockaddr_in server_addr;

    server_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    printf("Socket created successfully.\n");

    server_addr.sin_family = AF_INET;
 #include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 9410
#define BUFFER_SIZE 1024

void *handle_client(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];

    printf("Client thread started.\n");

    memset(buffer, 0, BUFFER_SIZE);

    int bytes_received = recv(
        client_fd,
        buffer,
        BUFFER_SIZE - 1,
        0
    );

    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';

        printf("Client says: %s", buffer);

        char response[] = "Hello Client\n";

        send(
            client_fd,
            response,
            strlen(response),
            0
        );
    }

    close(client_fd);

    printf("Client thread finished.\n");

    return NULL;
}

int main()
{
    int server_fd;

    struct sockaddr_in server_addr;

    server_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    printf("Socket created successfully.\n");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(
        server_fd,
        (struct sockaddr *)&server_addr,
        sizeof(server_addr)
    ) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    printf("Server bound to port %d.\n", PORT);

    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("RemoteOps Agent Started\n");
    printf("Listening on port %d\n", PORT);
    printf("SID:8652\n");

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(
            server_fd,
            (struct sockaddr *)&client_addr,
            &client_len
        );

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf("Client connected.\n");

        int *client_socket = malloc(sizeof(int));

        if (client_socket == NULL)
        {
            perror("malloc");
            close(client_fd);
            continue;
        }

        *client_socket = client_fd;

        pthread_t thread;

        if (pthread_create(
            &thread,
            NULL,
            handle_client,
            client_socket
        ) != 0)
        {
            perror("pthread_create");
            close(client_fd);
            free(client_socket);
            continue;
        }

        pthread_detach(thread);
    }

    close(server_fd);

 #include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 9410
#define BUFFER_SIZE 1024

void *handle_client(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];

    printf("Client thread started.\n");

    memset(buffer, 0, BUFFER_SIZE);

    int bytes_received = recv(
        client_fd,
        buffer,
        BUFFER_SIZE - 1,
        0
    );

    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';

        printf("Client says: %s", buffer);

        char response[] = "Hello Client\n";

        send(
            client_fd,
            response,
            strlen(response),
            0
        );
    }

    close(client_fd);

    printf("Client thread finished.\n");

    return NULL;
}

int main()
{
    int server_fd;

    struct sockaddr_in server_addr;

    server_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    printf("Socket created successfully.\n");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(
        server_fd,
        (struct sockaddr *)&server_addr,
        sizeof(server_addr)
    ) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    printf("Server bound to port %d.\n", PORT);

    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("RemoteOps Agent Started\n");
    printf("Listening on port %d\n", PORT);
    printf("SID:8652\n");

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(
            server_fd,
            (struct sockaddr *)&client_addr,
            &client_len
        );

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf("Client connected.\n");

        int *client_socket = malloc(sizeof(int));

        if (client_socket == NULL)
        {
            perror("malloc");
            close(client_fd);
            continue;
        }

        *client_socket = client_fd;

        pthread_t thread;

        if (pthread_create(
            &thread,
            NULL,
            handle_client,
            client_socket
        ) != 0)
        {
            perror("pthread_create");
            close(client_fd);
            free(client_socket);
            continue;
        }

        pthread_detach(thread);
    }

    close(server_fd);

    return 0;
}   return 0;
}   server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(
        server_fd,
        (struct sockaddr *)&server_addr,
        sizeof(server_addr)
    ) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    printf("Server bound to port %d.\n", PORT);

    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("RemoteOps Agent Started\n");
    printf("Listening on port %d\n", PORT);
    printf("SID:8652\n");

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(
            server_fd,
            (struct sockaddr *)&client_addr,
            &client_len
        );

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf("Client connected.\n");

        int *client_socket = malloc(sizeof(int));

        if (client_socket == NULL)
        {
            perror("malloc");
            close(client_fd);
            continue;
        }

        *client_socket = client_fd;

        pthread_t thread;

        if (pthread_create(
            &thread,
            NULL,
            handle_client,
            client_socket
        ) != 0)
        {
            perror("pthread_create");
            close(client_fd);
            free(client_socket);
            continue;
        }

        pthread_detach(thread);
    }

    close(server_fd);

    return 0;
}
