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
 * Handle LISTPROC command
 *
 * Gets currently running processes using
 * the Linux "ps" command.
 */
void handle_listproc(int client_fd)
{
    FILE *process_pipe;

    char line[256];

    char response[BUFFER_SIZE];


    /*
     * Open a pipe to the ps command.
     *
     * ps -eo pid,user,comm
     *
     * PID  = Process ID
     * USER = Process owner
     * COMM = Command/process name
     */
    process_pipe = popen(
        "ps -eo pid,user,comm --sort=pid",
        "r"
    );

    if (process_pipe == NULL)
    {
        char error_response[] =
            "ERR 003 LISTPROC_FAILED\n";

        send(
            client_fd,
            error_response,
            strlen(error_response),
            0
        );

        printf("LISTPROC failed.\n");

        return;
    }


    /*
     * Send LISTPROC header first.
     */
    snprintf(
        response,
        sizeof(response),
        "OK PROCS SID:%s\n",
        SID
    );

    send(
        client_fd,
        response,
        strlen(response),
        0
    );


    /*
     * Read process information line by line.
     */
    while (fgets(
        line,
        sizeof(line),
        process_pipe
    ) != NULL)
    {
        /*
         * Send each process line
         * to the Controller.
         */
        send(
            client_fd,
            line,
            strlen(line),
            0
        );
    }


    /*
     * Close the pipe.
     */
    int status = pclose(process_pipe);

    if (status == -1)
    {
        printf("LISTPROC pclose failed.\n");
    }


    /*
     * Send end marker.
     *
     * Controller needs this marker to know
     * that the process list has finished.
     */
    char end_response[] =
        "END PROCS\n";

    send(
        client_fd,
        end_response,
        strlen(end_response),
        0
    );


    printf(
        "LISTPROC sent successfully.\n"
    );
}


/*
 * Handle EXEC command
 *
 * Allowed commands:
 *
 * EXEC DATE
 * EXEC UPTIME
 * EXEC DISKFREE
 * EXEC HOSTNAME
 * EXEC WHOAMI
 *
 * Only whitelisted commands are allowed.
 */
void handle_exec(int client_fd, const char *command)
{
    const char *linux_command = NULL;

    char response[BUFFER_SIZE];
    char output[BUFFER_SIZE];

    FILE *command_pipe;

    size_t used = 0;


    /*
     * Get the command after "EXEC "
     *
     * Example:
     *
     * EXEC HOSTNAME
     *
     * command + 5 gives:
     *
     * HOSTNAME
     */
    const char *requested_command = command + 5;


    /*
     * Check the command against
     * the allowed command list.
     */

    if (strcmp(requested_command, "DATE") == 0)
    {
        linux_command = "date";
    }
    else if (strcmp(requested_command, "UPTIME") == 0)
    {
        linux_command = "uptime";
    }
    else if (strcmp(requested_command, "DISKFREE") == 0)
    {
        linux_command = "df -h";
    }
    else if (strcmp(requested_command, "HOSTNAME") == 0)
    {
        linux_command = "hostname";
    }
    else if (strcmp(requested_command, "WHOAMI") == 0)
    {
        linux_command = "whoami";
    }
    else
    {
        /*
         * Command is not in whitelist.
         */
        snprintf(
            response,
            sizeof(response),
            "ERR 002 COMMAND_NOT_ALLOWED\n"
        );

        send(
            client_fd,
            response,
            strlen(response),
            0
        );

        printf(
            "EXEC rejected: %s\n",
            requested_command
        );

        return;
    }


    /*
     * Execute only the fixed Linux command.
     *
     * Because linux_command comes only from
     * the whitelist above, arbitrary commands
     * cannot be executed.
     */
    command_pipe = popen(
        linux_command,
        "r"
    );

    if (command_pipe == NULL)
    {
        snprintf(
            response,
            sizeof(response),
            "ERR 004 EXEC_FAILED\n"
        );

        send(
            client_fd,
            response,
            strlen(response),
            0
        );

        printf("EXEC failed.\n");

        return;
    }


    /*
     * Clear output buffer.
     */
    memset(
        output,
        0,
        sizeof(output)
    );


    /*
     * Read command output.
     */
    while (
        used < sizeof(output) - 1 &&
        fgets(
            output + used,
            sizeof(output) - used,
            command_pipe
        ) != NULL
    )
    {
        used = strlen(output);
    }


    /*
     * Close command pipe.
     */
    int status = pclose(command_pipe);

    if (status == -1)
    {
        printf("EXEC pclose failed.\n");
    }


    /*
     * Remove trailing newline.
     */
    while (
        used > 0 &&
        (
            output[used - 1] == '\n' ||
            output[used - 1] == '\r'
        )
    )
    {
        output[used - 1] = '\0';
        used--;
    }


    /*
     * Create EXEC response.
     */
    snprintf(
        response,
        sizeof(response),
        "OK EXEC_RESULT %s\n",
        output
    );


    /*
     * Send result to Controller.
     */
    send(
        client_fd,
        response,
        strlen(response),
        0
    );


    printf(
        "EXEC successful: %s -> %s\n",
        requested_command,
        output
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
            printf("Client disconnected.\n");
            break;
        }

        buffer[bytes_received] = '\0';


        /*
         * Remove newline
         */
        buffer[strcspn(
            buffer,
            "\r\n"
        )] = '\0';


        printf(
            "Received: %s\n",
            buffer
        );


        /*
         * ================================
         * AUTHENTICATION
         * ================================
         */
        if (strncmp(
            buffer,
            "AUTH ",
            5
        ) == 0)
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
            if (strcmp(
                received_token,
                AUTH_TOKEN
            ) == 0)
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

                printf(
                    "AUTH failed.\n"
                );
            }

            continue;
        }


        /*
         * ================================
         * QUIT
         * ================================
         */
        if (strcmp(
            buffer,
            "QUIT"
        ) == 0)
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
        if (strcmp(
            buffer,
            "SYSINFO"
        ) == 0)
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
         * ================================
         * LISTPROC
         * ================================
         */
        if (strcmp(
            buffer,
            "LISTPROC"
        ) == 0)
        {
            handle_listproc(client_fd);

            continue;
        }


        /*
         * ================================
         * EXEC
         * ================================
         *
         * Allowed:
         *
         * EXEC DATE
         * EXEC UPTIME
         * EXEC DISKFREE
         * EXEC HOSTNAME
         * EXEC WHOAMI
         */
        if (strncmp(
            buffer,
            "EXEC ",
            5
        ) == 0)
        {
            /*
             * Check whether a command
             * was actually provided.
             */
            if (strlen(buffer) <= 5)
            {
                char response[] =
                    "ERR 002 COMMAND_NOT_ALLOWED\n";

                send(
                    client_fd,
                    response,
                    strlen(response),
                    0
                );

                printf(
                    "EXEC rejected: empty command.\n"
                );

                continue;
            }


            /*
             * Handle EXEC command.
             */
            handle_exec(
                client_fd,
                buffer
            );

            continue;
        }


        /*
         * ================================
         * UNKNOWN COMMAND
         * ================================
         */
        char response[] =
            "ERR 005 UNKNOWN_COMMAND\n";

        send(
            client_fd,
            response,
            strlen(response),
            0
        );

        printf(
            "Unknown command received: %s\n",
            buffer
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
    server_addr.sin_family =
        AF_INET;

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
    if (listen(
        server_fd,
        5
    ) < 0)
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

    printf(
        "LISTPROC           : Enabled\n"
    );

    printf(
        "EXEC               : Enabled\n"
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
