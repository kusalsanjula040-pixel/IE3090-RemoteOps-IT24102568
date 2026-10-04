
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9410
#define BUFFER_SIZE 1024

int main()
{
    int client_fd;

    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE];


    /*
     * 1. Create socket
     */
    client_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (client_fd < 0)
    {
        perror("socket");
        return 1;
    }

    printf("Socket created successfully.\n");


    /*
     * 2. Configure Agent address
     *
     * 127.0.0.1 means Agent is running
     * on the same CentOS machine.
     */
    server_addr.sin_family = AF_INET;

    server_addr.sin_port = htons(PORT);


    if (inet_pton(
        AF_INET,
        "127.0.0.1",
        &server_addr.sin_addr
    ) <= 0)
    {
        perror("inet_pton");

        close(client_fd);

        return 1;
    }


    /*
     * 3. Connect to Agent
     */
    if (connect(
        client_fd,
        (struct sockaddr *)&server_addr,
        sizeof(server_addr)
    ) < 0)
    {
        perror("connect");

        close(client_fd);

        return 1;
    }


    printf("Connected to RemoteOps Agent.\n");


    /*
     * Command loop
     */
    while (1)
    {
        printf("\nRemoteOps> ");

        fflush(stdout);


        /*
         * Get command from user
         */
        if (fgets(
            buffer,
            BUFFER_SIZE,
            stdin
        ) == NULL)
        {
            break;
        }


        /*
         * Send command to Agent
         */
        send(
            client_fd,
            buffer,
            strlen(buffer),
            0
        );


        /*
         * Receive response
         */
        memset(
            buffer,
            0,
            BUFFER_SIZE
        );


        int bytes_received = recv(
            client_fd,
            buffer,
            BUFFER_SIZE - 1,
            0
        );


        if (bytes_received <= 0)
        {
            printf("Agent disconnected.\n");
            break;
        }


        buffer[bytes_received] = '\0';


        printf("Agent: %s", buffer);


        /*
         * Stop Controller if QUIT
         */
        if (strncmp(
            buffer,
            "OK BYE",
            6
        ) == 0)
        {
            break;
        }
    }


    /*
     * Close socket
     */
    close(client_fd);

    printf("Controller closed.\n");

    return 0;
}

