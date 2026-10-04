
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 9410
#define BUFFER_SIZE 1024

#define AUTH_TOKEN "OPS-2568"
#define SID "8652"


/*
 * Handle one connected Controller
 */
void *handle_client(void *arg)
{
    int client_fd = *(int *)arg;

    free(arg);

    char buffer[BUFFER_SIZE];

    int authenticated = 0;

    printf("Client thread started.\n");


    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);

        int bytes_received = recv(
            client_fd,
            buffer,
            BUFFER_SIZE - 1,
            0
        );

        if (bytes_received <= 0)
        {
            printf("Client disconnected.\n");
            break;
        }

        buffer[bytes_received] = '\0';

        /*
         * Remove newline if Controller sends one
         */
        buffer[strcspn(buffer, "\r\n")] = '\0';

        printf("Received: %s\n", buffer);


        /*
         * AUTHENTICATION
         */
        if (strncmp(buffer, "AUTH ", 5) == 0)
        {
            char received_token[100];

            memset(received_token, 0, sizeof(received_token));

            /*
             * Get token after "AUTH "
             */
            sscanf(buffer + 5, "%99s", received_token);


            /*
             * Check authentication token
             */
            if (strcmp(received_token, AUTH_TOKEN) == 0)
            {
                authenticated = 1;

                char response[BUFFER_SIZE];

                snprintf(
                    response,
                    sizeof(response),
                    "OK AUTHENTICATED SID:%s\n",
                    SID
                );

                send(
                    client_fd,
                    response,
                    strlen(response),
                    0
                );

                printf("AUTH successful. SID:%s\n", SID);
            }
            else
            {
                authenticated = 0;

                char response[BUFFER_SIZE];

                snprintf(
                    response,
                    sizeof(response),
                    "ERR 001 AUTH_FAILED SID:%s\n",
                    SID
                );

                send(
                    client_fd,
                    response,
                    strlen(response),
                    0
                );

                printf("AUTH failed.\n");
            }

            continue;
        }


        /*
         * QUIT
         */
        if (strcmp(buffer, "QUIT") == 0)
        {
            char response[BUFFER_SIZE];

            snprintf(
                response,
                sizeof(response),
                "OK BYE SID:%s\n",
                SID
            );

            send(
                client_fd,
                response,
                strlen(response),
                0
            );

            printf("Client requested QUIT.\n");

            break;
        }


        /*
         * Reject commands if not authenticated
         */
        if (authenticated == 0)
        {
            char response[] = "ERR 001 AUTH_REQUIRED\n";

            send(
                client_fd,
                response,
                strlen(response),
                0
            );

            printf("Command rejected - authentication required.\n");

            continue;
        }


        /*
         * Commands after successful authentication
         *
         * These will be implemented later:
         *
         * SYSINFO
         * LISTPROC
         * EXEC
         * PUT
         * GET
         * MONITOR START
         * MONITOR STOP
         */


        char response[] = "OK COMMAND_RECEIVED\n";

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


    /*
     * 1. Create socket
     */
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


    /*
     * Allow port reuse
     */
    int opt = 1;

    setsockopt(
        server_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt)
    );


    /*
     * 2. Configure server address
     */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);


    /*
     * 3. Bind
     */
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


    /*
     * 4. Listen
     */
    if (listen(server_fd, 5) < 0)
    {
        perror("listen");

        close(server_fd);

        return 1;
    }


    printf("\n");
    printf("=================================\n");
    printf("      RemoteOps Agent Started\n");
    printf("=================================\n");
    printf("Listening on port : %d\n", PORT);
    printf("SID                : %s\n", SID);
    printf("Authentication     : Enabled\n");
    printf("=================================\n\n");


    /*
     * 5. Accept multiple Controllers
     */
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


        printf("New Controller connected.\n");


        /*
         * Allocate memory for client socket
         */
        int *client_socket = malloc(sizeof(int));

        if (client_socket == NULL)
        {
            perror("malloc");

            close(client_fd);

            continue;
        }


        *client_socket = client_fd;


        /*
         * Create thread
         */
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


        /*
         * Thread does not need to be joined
         */
        pthread_detach(thread);
    }


    close(server_fd);

    return 0;
}

