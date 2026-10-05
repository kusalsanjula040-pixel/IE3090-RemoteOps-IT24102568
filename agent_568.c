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
 * Get CPU load from /proc/loadavg
 */
double get_cpu_load()
{
    FILE *fp;
    double load1;

    fp = fopen("/proc/loadavg", "r");

    if (fp == NULL)
    {
        return -1;
    }

    if (fscanf(fp, "%lf", &load1) != 1)
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    return load1;
}


/*
 * Get memory usage percentage
 */
int get_memory_usage()
{
    FILE *fp;
    char line[256];

    long mem_total = 0;
    long mem_available = 0;

    fp = fopen("/proc/meminfo", "r");

    if (fp == NULL)
    {
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (sscanf(line, "MemTotal: %ld kB", &mem_total) == 1)
        {
            continue;
        }

        if (sscanf(line, "MemAvailable: %ld kB", &mem_available) == 1)
        {
            continue;
        }
    }

    fclose(fp);

    if (mem_total == 0)
    {
        return -1;
    }

    long mem_used = mem_total - mem_available;

    return (int)((mem_used * 100) / mem_total);
}


/*
 * Get system uptime in seconds
 */
long get_uptime()
{
    FILE *fp;
    double uptime;

    fp = fopen("/proc/uptime", "r");

    if (fp == NULL)
    {
        return -1;
    }

    if (fscanf(fp, "%lf", &uptime) != 1)
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    return (long)uptime;
}


/*
 * Convert uptime seconds to HH:MM:SS
 */
void format_uptime(long seconds, char *buffer, size_t size)
{
    long hours;
    long minutes;
    long secs;

    hours = seconds / 3600;

    minutes = (seconds % 3600) / 60;

    secs = seconds % 60;

    snprintf(
        buffer,
        size,
        "%02ld:%02ld:%02ld",
        hours,
        minutes,
        secs
    );
}


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
         * Remove newline
         */
        buffer[strcspn(buffer, "\r\n")] = '\0';

        printf("Received: %s\n", buffer);


        /*
         * AUTHENTICATION
         */
        if (strncmp(buffer, "AUTH ", 5) == 0)
        {
            char received_token[100];

            memset(
                received_token,
                0,
                sizeof(received_token)
            );


            /*
             * Get token after "AUTH "
             */
            sscanf(
                buffer + 5,
                "%99s",
                received_token
            );


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

                printf(
                    "AUTH successful. SID:%s\n",
                    SID
                );
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

            printf(
                "Client requested QUIT.\n"
            );

            break;
        }


        /*
         * Reject commands if not authenticated
         */
        if (authenticated == 0)
        {
            char response[] =
                "ERR 001 AUTH_REQUIRED\n";

            send(
                client_fd,
                response,
                strlen(response),
                0
            );

            printf(
                "Command rejected - "
                "authentication required.\n"
            );

            continue;
        }


        /*
         * ================================
         * SYSINFO
         * ================================
         */
        if (strcmp(buffer, "SYSINFO") == 0)
        {
            double cpu;
            int memory;
            long uptime_seconds;

            char uptime_text[32];

            char response[BUFFER_SIZE];


            /*
             * Get system information
             */
            cpu = get_cpu_load();

            memory = get_memory_usage();

            uptime_seconds = get_uptime();


            /*
             * Check if information
             * was successfully retrieved
             */
            if (cpu < 0 ||
                memory < 0 ||
                uptime_seconds < 0)
            {
                char error_response[] =
                    "ERR SYSINFO_FAILED\n";

                send(
                    client_fd,
                    error_response,
                    strlen(error_response),
                    0
                );

                printf(
                    "SYSINFO failed.\n"
                );

                continue;
            }


            /*
             * Format uptime
             */
            format_uptime(
                uptime_seconds,
                uptime_text,
                sizeof(uptime_text)
            );


            /*
             * Create SYSINFO response
             */
            snprintf(
                response,
                sizeof(response),
                "OK SYSINFO CPU:%.2f "
                "MEMORY:%d%% "
                "UPTIME:%s "
                "SID:%s\n",
                cpu,
                memory,
                uptime_text,
                SID
            );


            /*
             * Send SYSINFO response
             */
            send(
                client_fd,
                response,
                strlen(response),
                0
            );


            printf(
                "SYSINFO sent: %s",
                response
            );

            continue;
        }


        /*
         * Other commands will be added later:
         *
         * LISTPROC
         * EXEC
         * PUT
         * GET
         * MONITOR START
         * MONITOR STOP
         */


        /*
         * Temporary response
         */
        char response[] =
            "OK COMMAND_RECEIVED\n";

        send(
            client_fd,
            response,
            strlen(response),
            0
        );
    }


    close(client_fd);

    printf(
        "Client thread finished.\n"
    );

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

    printf(
        "Socket created successfully.\n"
    );


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

    server_addr.sin_addr.s_addr =
        INADDR_ANY;

    server_addr.sin_port =
        htons(PORT);


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

    printf(
        "Server bound to port %d.\n",
        PORT
    );


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
    printf(
        "Listening on port : %d\n",
        PORT
    );
    printf(
        "SID                : %s\n",
        SID
    );
    printf(
        "Authentication     : Enabled\n"
    );
    printf(
        "SYSINFO            : Enabled\n"
    );
    printf("=================================\n\n");


    /*
     * 5. Accept multiple Controllers
     */
    while (1)
    {
        struct sockaddr_in client_addr;

        socklen_t client_len =
            sizeof(client_addr);


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


        printf(
            "New Controller connected.\n"
        );


        /*
         * Allocate memory for client socket
         */
        int *client_socket =
            malloc(sizeof(int));

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
         * Thread does not need
         * to be joined
         */
        pthread_detach(thread);
    }


    close(server_fd);

    return 0;
}

