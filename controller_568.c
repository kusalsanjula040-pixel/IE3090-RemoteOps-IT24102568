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

    printf(
        "Socket created successfully.\n"
    );


    /*
     * 2. Configure Agent address
     *
     * 127.0.0.1 means Agent is running
     * on the same CentOS machine.
     */
    server_addr.sin_family =
        AF_INET;

    server_addr.sin_port =
        htons(PORT);


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


    printf(
        "Connected to RemoteOps Agent.\n"
    );


    /*
     * ================================
     * Command loop
     * ================================
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
         * Keep a copy of the command.
         *
         * We need this because buffer will
         * later be cleared before recv().
         */
        char command[BUFFER_SIZE];

        strncpy(
            command,
            buffer,
            BUFFER_SIZE - 1
        );

        command[BUFFER_SIZE - 1] = '\0';


        /*
         * Remove newline from command
         *
         * Example:
         *
         * "LISTPROC\n"
         *
         * becomes:
         *
         * "LISTPROC"
         */
        command[strcspn(
            command,
            "\r\n"
        )] = '\0';


        /*
         * Send command to Agent
         */
        int bytes_sent = send(
            client_fd,
            buffer,
            strlen(buffer),
            0
        );


        if (bytes_sent < 0)
        {
            perror("send");
            break;
        }


        /*
         * ====================================
         * LISTPROC
         * ====================================
         *
         * LISTPROC returns multiple responses.
         *
         * Agent:
         *
         * OK PROCS
         * process 1
         * process 2
         * process 3
         * ...
         * END PROCS
         *
         * Therefore receive until END PROCS.
         */
        if (strcmp(
            command,
            "LISTPROC"
        ) == 0)
        {
            while (1)
            {
                memset(
                    buffer,
                    0,
                    BUFFER_SIZE
                );


                /*
                 * Receive process information
                 */
                int bytes_received = recv(
                    client_fd,
                    buffer,
                    BUFFER_SIZE - 1,
                    0
                );


                if (bytes_received <= 0)
                {
                    printf(
                        "Agent disconnected.\n"
                    );

                    close(client_fd);

                    return 1;
                }


                buffer[bytes_received] = '\0';


                /*
                 * Display process information
                 */
                printf(
                    "%s",
                    buffer
                );


                /*
                 * Check end marker
                 */
                if (strstr(
                    buffer,
                    "END PROCS"
                ) != NULL)
                {
                    break;
                }
            }


            /*
             * LISTPROC completed.
             */
            continue;
        }


        /*
         * ====================================
         * EXEC
         * ====================================
         *
         * Example:
         *
         * EXEC DATE
         * EXEC UPTIME
         * EXEC DISKFREE
         * EXEC HOSTNAME
         * EXEC WHOAMI
         *
         * Agent sends one response:
         *
         * OK EXEC_RESULT ...
         *
         * OR
         *
         * ERR 002 COMMAND_NOT_ALLOWED
         */
        if (strncmp(
            command,
            "EXEC ",
            5
        ) == 0)
        {
            /*
             * Check whether a command was
             * actually provided after EXEC.
             */
            if (strlen(command) <= 5)
            {
                printf(
                    "Usage: EXEC <command>\n"
                );

                continue;
            }


            memset(
                buffer,
                0,
                BUFFER_SIZE
            );


            /*
             * Receive EXEC response
             */
            int bytes_received = recv(
                client_fd,
                buffer,
                BUFFER_SIZE - 1,
                0
            );


            if (bytes_received <= 0)
            {
                printf(
                    "Agent disconnected.\n"
                );

                break;
            }


            buffer[bytes_received] = '\0';


            /*
             * Display Agent response
             */
            printf(
                "Agent: %s",
                buffer
            );


            continue;
        }


        /*
         * ====================================
         * Normal command response
         * ====================================
         *
         * AUTH
         * SYSINFO
         * QUIT
         * and other single-response commands.
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
            printf(
                "Agent disconnected.\n"
            );

            break;
        }


        buffer[bytes_received] = '\0';


        printf(
            "Agent: %s",
            buffer
        );


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


    printf(
        "Controller closed.\n"
    );


    return 0;
}
