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

    // 1. Create socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (client_fd < 0)
    {
        perror("socket");
        return 1;
    }

    printf("Socket created successfully.\n");

    // 2. Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Server IP
    if (inet_pton(AF_INET, "127.0.0.1",
                  &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(client_fd);
        return 1;
    }

    // 3. Connect to server
    if (connect(client_fd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(client_fd);
        return 1;
    }

    printf("Connected to server.\n");

    // 4. Send message
    char message[] = "Hello Server\n";

    send(client_fd,
         message,
         strlen(message),
         0);

    printf("Message sent.\n");

    // 5. Receive server response
    memset(buffer, 0, BUFFER_SIZE);

    int bytes_received = recv(client_fd,
                              buffer,
                              BUFFER_SIZE - 1,
                              0);

    if (bytes_received < 0)
    {
        perror("recv");
    }
    else
    {
        buffer[bytes_received] = '\0';

        printf("Server says: %s", buffer);
    }

    // 6. Close socket
    close(client_fd);

    printf("Client closed.\n");

    return 0;
}
