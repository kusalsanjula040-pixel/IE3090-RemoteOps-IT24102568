#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#define PORT 9410
#define BUFFER_SIZE 1024

#define AUTH_TOKEN "OPS-2568"
#define SID "8652"

#define STORAGE_PARENT "./agentfiles"
#define STORAGE_DIR "./agentfiles/IT24102568"

#define UDP_PORT 9410

#define ERR_LINE_TOO_LONG       -2
#define ERR_CONNECTION_CLOSED   -3

volatile int monitor_running = 0;
volatile int monitor_stop_requested = 0;

char monitor_controller_ip[INET_ADDRSTRLEN];
char connected_controller_ip[INET_ADDRSTRLEN];

pthread_t monitor_thread;

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;


/*
 * ============================================================
 * LOG EVENT
 * ============================================================
 */
void log_event(const char *event)
{
    FILE *log_file;
    time_t now;
    struct tm time_info;
    char time_text[32];

    now = time(NULL);

    if (localtime_r(&now, &time_info) == NULL)
    {
        return;
    }

    strftime(
        time_text,
        sizeof(time_text),
        "%H:%M:%S",
        &time_info
    );

    pthread_mutex_lock(&log_mutex);

    log_file = fopen(
        "remoteops_IT24102568.log",
        "a"
    );

    if (log_file != NULL)
    {
        fprintf(
            log_file,
            "[%s] %s\n",
            time_text,
            event
        );

        fclose(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}


/*
 * ============================================================
 * SEND ALL
 * ============================================================
 */
int send_all(
    int socket_fd,
    const void *data,
    size_t total_bytes
)
{
    size_t total_sent = 0;

    const char *buffer =
        (const char *)data;

    while (total_sent < total_bytes)
    {
        ssize_t bytes_sent =
            send(
                socket_fd,
                buffer + total_sent,
                total_bytes - total_sent,
                0
            );

        if (bytes_sent < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return -1;
        }

        if (bytes_sent == 0)
        {
            return -1;
        }

        total_sent +=
            (size_t)bytes_sent;
    }

    return 0;
}


/*
 * ============================================================
 * RECEIVE ALL
 * ============================================================
 */
int recv_all(
    int socket_fd,
    void *data,
    size_t total_bytes
)
{
    size_t total_received = 0;

    char *buffer =
        (char *)data;

    while (total_received < total_bytes)
    {
        ssize_t bytes_received =
            recv(
                socket_fd,
                buffer + total_received,
                total_bytes - total_received,
                0
            );

        if (bytes_received < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return -1;
        }

        if (bytes_received == 0)
        {
            return -1;
        }

        total_received +=
            (size_t)bytes_received;
    }

    return 0;
}


/*
 * ============================================================
 * UINT64 NETWORK BYTE ORDER
 * ============================================================
 */
uint64_t htonll(uint64_t value)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

    return
        ((uint64_t)htonl(
            (uint32_t)(value & 0xFFFFFFFFULL)
        ) << 32)
        |
        htonl(
            (uint32_t)(value >> 32)
        );

#else

    return value;

#endif
}


uint64_t ntohll(uint64_t value)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

    return
        ((uint64_t)ntohl(
            (uint32_t)(value & 0xFFFFFFFFULL)
        ) << 32)
        |
        ntohl(
            (uint32_t)(value >> 32)
        );

#else

    return value;

#endif
}


/*
 * ============================================================
 * RECEIVE LINE
 * ============================================================
 *
 * Return values:
 *
 * >= 0  : number of characters received
 * -1    : socket error / disconnect
 * -2    : line too long
 */
int recv_line(
    int socket_fd,
    char *buffer,
    size_t buffer_size
)
{
    size_t index = 0;
    int too_long = 0;

    if (
        buffer == NULL ||
        buffer_size < 2
    )
    {
        return -1;
    }

    while (1)
    {
        char character;

        ssize_t result =
            recv(
                socket_fd,
                &character,
                1,
                0
            );

        if (result < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            return -1;
        }

        if (result == 0)
        {
            return -1;
        }

        if (character == '\n')
        {
            break;
        }

        if (character == '\r')
        {
            continue;
        }

        if (index < buffer_size - 1)
        {
            buffer[index++] =
                character;
        }
        else
        {
            too_long = 1;
        }
    }

    buffer[index] = '\0';

    if (too_long)
    {
        return ERR_LINE_TOO_LONG;
    }

    return (int)index;
}


/*
 * ============================================================
 * SEND ERROR RESPONSE
 * ============================================================
 */
void send_error(
    int client_fd,
    const char *error_message
)
{
    if (
        send_all(
            client_fd,
            error_message,
            strlen(error_message)
        ) < 0
    )
    {
        printf(
            "Failed to send error response: %s",
            error_message
        );
    }
}


/*
 * ============================================================
 * ENSURE STORAGE DIRECTORY
 * ============================================================
 */
int ensure_storage_directory()
{
    if (
        mkdir(
            STORAGE_PARENT,
            0755
        ) < 0 &&
        errno != EEXIST
    )
    {
        return -1;
    }

    if (
        mkdir(
            STORAGE_DIR,
            0755
        ) < 0 &&
        errno != EEXIST
    )
    {
        return -1;
    }

    return 0;
}


/*
 * ============================================================
 * GET CPU LOAD
 * ============================================================
 */
double get_cpu_load()
{
    FILE *fp;
    double load1;

    fp = fopen(
        "/proc/loadavg",
        "r"
    );

    if (fp == NULL)
    {
        return -1;
    }

    if (
        fscanf(
            fp,
            "%lf",
            &load1
        ) != 1
    )
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    return load1;
}


/*
 * ============================================================
 * GET MEMORY USAGE
 * ============================================================
 */
int get_memory_usage()
{
    FILE *fp;

    char line[256];

    long mem_total = 0;
    long mem_available = 0;

    fp = fopen(
        "/proc/meminfo",
        "r"
    );

    if (fp == NULL)
    {
        return -1;
    }

    while (
        fgets(
            line,
            sizeof(line),
            fp
        ) != NULL
    )
    {
        if (
            sscanf(
                line,
                "MemTotal: %ld kB",
                &mem_total
            ) == 1
        )
        {
            continue;
        }

        if (
            sscanf(
                line,
                "MemAvailable: %ld kB",
                &mem_available
            ) == 1
        )
        {
            continue;
        }
    }

    fclose(fp);

    if (
        mem_total <= 0 ||
        mem_available < 0
    )
    {
        return -1;
    }

    long mem_used =
        mem_total - mem_available;

    if (mem_used < 0)
    {
        mem_used = 0;
    }

    return (int)(
        (mem_used * 100) /
        mem_total
    );
}


/*
 * ============================================================
 * GET SYSTEM UPTIME
 * ============================================================
 */
long get_uptime()
{
    FILE *fp;

    double uptime;

    fp = fopen(
        "/proc/uptime",
        "r"
    );

    if (fp == NULL)
    {
        return -1;
    }

    if (
        fscanf(
            fp,
            "%lf",
            &uptime
        ) != 1
    )
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    return (long)uptime;
}


/*
 * ============================================================
 * FORMAT UPTIME
 * ============================================================
 */
void format_uptime(
    long seconds,
    char *buffer,
    size_t size
)
{
    long hours;
    long minutes;
    long secs;

    hours =
        seconds / 3600;

    minutes =
        (seconds % 3600) / 60;

    secs =
        seconds % 60;

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
 * ============================================================
 * LISTPROC
 * ============================================================
 */
void handle_listproc(
    int client_fd
)
{
    FILE *process_pipe;

    char line[256];

    char response[BUFFER_SIZE];

    process_pipe =
        popen(
            "ps -eo pid,user,comm --sort=pid",
            "r"
        );

    if (process_pipe == NULL)
    {
        send_error(
            client_fd,
            "ERR 003 LISTPROC_FAILED\n"
        );

        log_event(
            "LISTPROC failed: popen"
        );

        return;
    }

    snprintf(
        response,
        sizeof(response),
        "OK PROCS SID:%s\n",
        SID
    );

    if (
        send_all(
            client_fd,
            response,
            strlen(response)
        ) < 0
    )
    {
        pclose(process_pipe);
        return;
    }

    while (
        fgets(
            line,
            sizeof(line),
            process_pipe
        ) != NULL
    )
    {
        if (
            send_all(
                client_fd,
                line,
                strlen(line)
            ) < 0
        )
        {
            pclose(process_pipe);
            return;
        }
    }

    int status =
        pclose(process_pipe);

    if (
        status == -1
    )
    {
        log_event(
            "LISTPROC pclose failed"
        );
    }

    send_all(
        client_fd,
        "END PROCS\n",
        strlen("END PROCS\n")
    );

    printf(
        "LISTPROC sent successfully.\n"
    );

    log_event(
        "LISTPROC requested"
    );
}


/*
 * ============================================================
 * EXEC
 * ============================================================
 */
void handle_exec(
    int client_fd,
    const char *command
)
{
    const char *linux_command = NULL;

    char response[BUFFER_SIZE];
    char output[BUFFER_SIZE];

    FILE *command_pipe;

    size_t used = 0;

    const char *requested_command =
        command + 5;

    if (
        strcmp(
            requested_command,
            "DATE"
        ) == 0
    )
    {
        linux_command = "date";
    }
    else if (
        strcmp(
            requested_command,
            "UPTIME"
        ) == 0
    )
    {
        linux_command = "uptime";
    }
    else if (
        strcmp(
            requested_command,
            "DISKFREE"
        ) == 0
    )
    {
        linux_command = "df -h";
    }
    else if (
        strcmp(
            requested_command,
            "HOSTNAME"
        ) == 0
    )
    {
        linux_command = "hostname";
    }
    else if (
        strcmp(
            requested_command,
            "WHOAMI"
        ) == 0
    )
    {
        linux_command = "whoami";
    }
    else
    {
        send_error(
            client_fd,
            "ERR 002 COMMAND_NOT_ALLOWED\n"
        );

        log_event(
            "EXEC rejected: command not allowed"
        );

        return;
    }

    command_pipe =
        popen(
            linux_command,
            "r"
        );

    if (command_pipe == NULL)
    {
        send_error(
            client_fd,
            "ERR 004 EXEC_FAILED\n"
        );

        log_event(
            "EXEC failed: popen"
        );

        return;
    }

    memset(
        output,
        0,
        sizeof(output)
    );

    while (
        used < sizeof(output) - 1 &&
        fgets(
            output + used,
            sizeof(output) - used,
            command_pipe
        ) != NULL
    )
    {
        used =
            strlen(output);
    }

    int status =
        pclose(command_pipe);

    if (
        status == -1
    )
    {
        send_error(
            client_fd,
            "ERR 004 EXEC_FAILED\n"
        );

        log_event(
            "EXEC failed: pclose"
        );

        return;
    }

    if (
        WIFEXITED(status) &&
        WEXITSTATUS(status) != 0
    )
    {
        send_error(
            client_fd,
            "ERR 004 EXEC_FAILED\n"
        );

        log_event(
            "EXEC failed: command exit status"
        );

        return;
    }

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

    snprintf(
        response,
        sizeof(response),
        "OK EXEC_RESULT %.990s\n",
        output
    );

    if (
        send_all(
            client_fd,
            response,
            strlen(response)
        ) < 0
    )
    {
        return;
    }

    printf(
        "EXEC successful: %s -> %s\n",
        requested_command,
        output
    );

    char log_message[BUFFER_SIZE];

    snprintf(
        log_message,
        sizeof(log_message),
        "EXEC %s",
        requested_command
    );

    log_event(
        log_message
    );
}


/*
 * ============================================================
 * VALIDATE FILENAME
 * ============================================================
 */
int valid_filename(
    const char *filename
)
{
    if (
        filename == NULL ||
        strlen(filename) == 0
    )
    {
        return 0;
    }

    if (
        strcmp(filename, ".") == 0 ||
        strcmp(filename, "..") == 0
    )
    {
        return 0;
    }

    if (
        strchr(filename, '/') != NULL ||
        strchr(filename, '\\') != NULL
    )
    {
        return 0;
    }

    return 1;
}


/*
 * ============================================================
 * PUT
 * ============================================================
 */
void handle_put(
    int client_fd,
    const char *command
)
{
    char filename[256];
    char filepath[512];
    char response[BUFFER_SIZE];

    if (
        sscanf(
            command + 4,
            "%255s",
            filename
        ) != 1
    )
    {
        send_error(
            client_fd,
            "ERR 006 INVALID_FILENAME\n"
        );

        log_event(
            "PUT failed: invalid filename"
        );

        return;
    }

    if (
        !valid_filename(filename)
    )
    {
        send_error(
            client_fd,
            "ERR 006 INVALID_FILENAME\n"
        );

        log_event(
            "PUT failed: invalid filename"
        );

        return;
    }

    if (
        ensure_storage_directory() < 0
    )
    {
        send_error(
            client_fd,
            "ERR 007 STORAGE_ERROR\n"
        );

        log_event(
            "PUT failed: storage directory"
        );

        return;
    }

    int path_result =
        snprintf(
            filepath,
            sizeof(filepath),
            "%s/%s",
            STORAGE_DIR,
            filename
        );

    if (
        path_result < 0 ||
        (size_t)path_result >= sizeof(filepath)
    )
    {
        send_error(
            client_fd,
            "ERR 006 INVALID_FILENAME\n"
        );

        return;
    }

    snprintf(
        response,
        sizeof(response),
        "OK READY SID:%s\n",
        SID
    );

    if (
        send_all(
            client_fd,
            response,
            strlen(response)
        ) < 0
    )
    {
        return;
    }

    uint64_t network_file_size;

    if (
        recv_all(
            client_fd,
            &network_file_size,
            sizeof(network_file_size)
        ) < 0
    )
    {
        log_event(
            "PUT failed: file size receive"
        );

        return;
    }

    uint64_t file_size =
        ntohll(network_file_size);

    FILE *output_file =
        fopen(
            filepath,
            "wb"
        );

    if (
        output_file == NULL
    )
    {
        send_error(
            client_fd,
            "ERR 007 STORAGE_ERROR\n"
        );

        log_event(
            "PUT failed: fopen"
        );

        return;
    }

    char file_buffer[BUFFER_SIZE];

    uint64_t remaining =
        file_size;

    while (
        remaining > 0
    )
    {
        size_t chunk_size =
            remaining > BUFFER_SIZE
                ? BUFFER_SIZE
                : (size_t)remaining;

        ssize_t bytes_received;

        do
        {
            bytes_received =
                recv(
                    client_fd,
                    file_buffer,
                    chunk_size,
                    0
                );
        }
        while (
            bytes_received < 0 &&
            errno == EINTR
        );

        if (
            bytes_received <= 0
        )
        {
            fclose(output_file);
            remove(filepath);

            log_event(
                "PUT failed: incomplete transfer"
            );

            return;
        }

        size_t bytes_written =
            fwrite(
                file_buffer,
                1,
                (size_t)bytes_received,
                output_file
            );

        if (
            bytes_written !=
            (size_t)bytes_received
        )
        {
            fclose(output_file);
            remove(filepath);

            send_error(
                client_fd,
                "ERR 007 STORAGE_ERROR\n"
            );

            log_event(
                "PUT failed: fwrite"
            );

            return;
        }

        remaining -=
            (uint64_t)bytes_received;
    }

    if (
        fclose(output_file) != 0
    )
    {
        remove(filepath);

        send_error(
            client_fd,
            "ERR 007 STORAGE_ERROR\n"
        );

        log_event(
            "PUT failed: fclose"
        );

        return;
    }

    snprintf(
        response,
        sizeof(response),
        "OK FILE_RECEIVED SID:%s\n",
        SID
    );

    send_all(
        client_fd,
        response,
        strlen(response)
    );

    printf(
        "PUT successful: %s (%llu bytes)\n",
        filename,
        (unsigned long long)file_size
    );

    char log_message[BUFFER_SIZE];

    snprintf(
        log_message,
        sizeof(log_message),
        "PUT %s",
        filename
    );

    log_event(
        log_message
    );
}


/*
 * ============================================================
 * GET
 * ============================================================
 */
void handle_get(
    int client_fd,
    const char *command
)
{
    char filename[256];
    char filepath[512];
    char response[BUFFER_SIZE];

    if (
        sscanf(
            command + 4,
            "%255s",
            filename
        ) != 1
    )
    {
        send_error(
            client_fd,
            "ERR 008 INVALID_FILENAME\n"
        );

        return;
    }

    if (
        !valid_filename(filename)
    )
    {
        send_error(
            client_fd,
            "ERR 008 INVALID_FILENAME\n"
        );

        log_event(
            "GET failed: invalid filename"
        );

        return;
    }

    int path_result =
        snprintf(
            filepath,
            sizeof(filepath),
            "%s/%s",
            STORAGE_DIR,
            filename
        );

    if (
        path_result < 0 ||
        (size_t)path_result >= sizeof(filepath)
    )
    {
        send_error(
            client_fd,
            "ERR 008 INVALID_FILENAME\n"
        );

        return;
    }

    FILE *input_file =
        fopen(
            filepath,
            "rb"
        );

    if (
        input_file == NULL
    )
    {
        send_error(
            client_fd,
            "ERR 009 FILE_NOT_FOUND\n"
        );

        printf(
            "GET: file not found: %s\n",
            filename
        );

        return;
    }

    if (
        fseek(
            input_file,
            0,
            SEEK_END
        ) != 0
    )
    {
        fclose(input_file);

        send_error(
            client_fd,
            "ERR 010 FILE_ERROR\n"
        );

        return;
    }

    long file_size_long =
        ftell(input_file);

    if (
        file_size_long < 0
    )
    {
        fclose(input_file);

        send_error(
            client_fd,
            "ERR 010 FILE_ERROR\n"
        );

        return;
    }

    rewind(input_file);

    uint64_t file_size =
        (uint64_t)file_size_long;

    snprintf(
        response,
        sizeof(response),
        "OK FILE_READY SID:%s SIZE:%llu\n",
        SID,
        (unsigned long long)file_size
    );

    if (
        send_all(
            client_fd,
            response,
            strlen(response)
        ) < 0
    )
    {
        fclose(input_file);
        return;
    }

    char file_buffer[BUFFER_SIZE];

    uint64_t remaining =
        file_size;

    while (
        remaining > 0
    )
    {
        size_t chunk_size =
            remaining > BUFFER_SIZE
                ? BUFFER_SIZE
                : (size_t)remaining;

        size_t bytes_read =
            fread(
                file_buffer,
                1,
                chunk_size,
                input_file
            );

        if (
            bytes_read == 0
        )
        {
            if (ferror(input_file))
            {
                fclose(input_file);

                send_error(
                    client_fd,
                    "ERR 010 FILE_ERROR\n"
                );

                log_event(
                    "GET failed: fread"
                );

                return;
            }

            break;
        }

        if (
            send_all(
                client_fd,
                file_buffer,
                bytes_read
            ) < 0
        )
        {
            fclose(input_file);
            return;
        }

        remaining -=
            (uint64_t)bytes_read;
    }

    fclose(input_file);

    if (
        remaining != 0
    )
    {
        send_error(
            client_fd,
            "ERR 010 FILE_ERROR\n"
        );

        return;
    }

    snprintf(
        response,
        sizeof(response),
        "OK FILE_SENT SID:%s\n",
        SID
    );

    send_all(
        client_fd,
        response,
        strlen(response)
    );

    printf(
        "GET successful: %s (%llu bytes)\n",
        filename,
        (unsigned long long)file_size
    );

    char log_message[BUFFER_SIZE];

    snprintf(
        log_message,
        sizeof(log_message),
        "GET %s",
        filename
    );

    log_event(
        log_message
    );
}


/*
 * ============================================================
 * UDP MONITOR WORKER
 * ============================================================
 */
void *monitor_worker(void *arg)
{
    (void)arg;

    int udp_fd =
        socket(
            AF_INET,
            SOCK_DGRAM,
            0
        );

    if (
        udp_fd < 0
    )
    {
        perror("monitor socket");

        log_event(
            "MONITOR failed: UDP socket"
        );

        monitor_running = 0;

        return NULL;
    }

    struct sockaddr_in controller_addr;

    memset(
        &controller_addr,
        0,
        sizeof(controller_addr)
    );

    controller_addr.sin_family =
        AF_INET;

    controller_addr.sin_port =
        htons(UDP_PORT);

    if (
        inet_pton(
            AF_INET,
            monitor_controller_ip,
            &controller_addr.sin_addr
        ) <= 0
    )
    {
        log_event(
            "MONITOR failed: invalid controller IP"
        );

        close(udp_fd);

        monitor_running = 0;

        return NULL;
    }

    while (
        !monitor_stop_requested
    )
    {
        double cpu =
            get_cpu_load();

        int memory =
            get_memory_usage();

        long uptime_seconds =
            get_uptime();

        char uptime_text[32];

        char monitor_message[BUFFER_SIZE];

        if (
            cpu >= 0 &&
            memory >= 0 &&
            uptime_seconds >= 0
        )
        {
            format_uptime(
                uptime_seconds,
                uptime_text,
                sizeof(uptime_text)
            );

            snprintf(
                monitor_message,
                sizeof(monitor_message),
                "MONITOR CPU=%.2f MEM=%d%% UPTIME=%s SID:%s",
                cpu,
                memory,
                uptime_text,
                SID
            );

            ssize_t sent =
                sendto(
                    udp_fd,
                    monitor_message,
                    strlen(monitor_message),
                    0,
                    (struct sockaddr *)&controller_addr,
                    sizeof(controller_addr)
                );

            if (
                sent < 0 &&
                errno != EINTR
            )
            {
                log_event(
                    "MONITOR UDP send failed"
                );
            }
        }
        else
        {
            log_event(
                "MONITOR metric collection failed"
            );
        }

        for (
            int i = 0;
            i < 2 &&
            !monitor_stop_requested;
            i++
        )
        {
            sleep(1);
        }
    }

    close(udp_fd);

    monitor_running = 0;

    return NULL;
}


/*
 * ============================================================
 * START MONITOR
 * ============================================================
 */
int start_monitor(
    const char *controller_ip
)
{
    if (
        controller_ip == NULL ||
        strlen(controller_ip) == 0
    )
    {
        return -1;
    }

    if (monitor_running)
    {
        return 1;
    }

    snprintf(
        monitor_controller_ip,
        sizeof(monitor_controller_ip),
        "%s",
        controller_ip
    );

    monitor_stop_requested = 0;

    monitor_running = 1;

    if (
        pthread_create(
            &monitor_thread,
            NULL,
            monitor_worker,
            NULL
        ) != 0
    )
    {
        monitor_running = 0;

        log_event(
            "MONITOR failed: pthread_create"
        );

        return -1;
    }

    return 0;
}


/*
 * ============================================================
 * STOP MONITOR
 * ============================================================
 */
void stop_monitor()
{
    if (!monitor_running)
    {
        return;
    }

    monitor_stop_requested = 1;

    pthread_join(
        monitor_thread,
        NULL
    );

    monitor_running = 0;
}


/*
 * ============================================================
 * HANDLE CLIENT
 * ============================================================
 */
void *handle_client(
    void *arg
)
{
    int client_fd =
        *(int *)arg;

    free(arg);

    char buffer[BUFFER_SIZE];

    int authenticated = 0;

    printf(
        "Client thread started.\n"
    );

    while (1)
    {
        memset(
            buffer,
            0,
            sizeof(buffer)
        );

        int line_result =
            recv_line(
                client_fd,
                buffer,
                sizeof(buffer)
            );

        if (
            line_result == ERR_LINE_TOO_LONG
        )
        {
            send_error(
                client_fd,
                "ERR 013 COMMAND_TOO_LONG\n"
            );

            log_event(
                "Command rejected: too long"
            );

            continue;
        }

        if (
            line_result < 0
        )
        {
            printf(
                "Client disconnected.\n"
            );

            log_event(
                "Client disconnected"
            );

            break;
        }

        if (
            strlen(buffer) == 0
        )
        {
            send_error(
                client_fd,
                "ERR 005 UNKNOWN_COMMAND\n"
            );

            continue;
        }

        printf(
            "Received: %s\n",
            buffer
        );


        /*
         * ====================================================
         * AUTH
         * ====================================================
         */
        if (
            strncmp(
                buffer,
                "AUTH ",
                5
            ) == 0
        )
        {
            char received_token[100];

            memset(
                received_token,
                0,
                sizeof(received_token)
            );

            if (
                sscanf(
                    buffer + 5,
                    "%99s",
                    received_token
                ) != 1
            )
            {
                authenticated = 0;

                send_error(
                    client_fd,
                    "ERR 001 AUTH_FAILED SID:8652\n"
                );

                continue;
            }

            if (
                strcmp(
                    received_token,
                    AUTH_TOKEN
                ) == 0
            )
            {
                authenticated = 1;

                char response[BUFFER_SIZE];

                snprintf(
                    response,
                    sizeof(response),
                    "OK AUTHENTICATED SID:%s\n",
                    SID
                );

                send_all(
                    client_fd,
                    response,
                    strlen(response)
                );

                log_event(
                    "AUTH successful"
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

                send_all(
                    client_fd,
                    response,
                    strlen(response)
                );

                log_event(
                    "AUTH failed"
                );
            }

            continue;
        }


        /*
         * ====================================================
         * QUIT
         * ====================================================
         */
        if (
            strcmp(
                buffer,
                "QUIT"
            ) == 0
        )
        {
            stop_monitor();

            char response[BUFFER_SIZE];

            snprintf(
                response,
                sizeof(response),
                "OK BYE SID:%s\n",
                SID
            );

            send_all(
                client_fd,
                response,
                strlen(response)
            );

            log_event(
                "Client requested QUIT"
            );

            break;
        }


        /*
         * ====================================================
         * AUTH REQUIRED
         * ====================================================
         */
        if (!authenticated)
        {
            send_error(
                client_fd,
                "ERR 001 AUTH_REQUIRED\n"
            );

            continue;
        }


        /*
         * ====================================================
         * MONITOR START
         * ====================================================
         */
        if (
            strcmp(
                buffer,
                "MONITOR START"
            ) == 0
        )
        {
            int result =
                start_monitor(
                    connected_controller_ip
                );

            char response[BUFFER_SIZE];

            if (result == 0)
            {
                snprintf(
                    response,
                    sizeof(response),
                    "OK MONITOR_STARTED SID:%s\n",
                    SID
                );

                log_event(
                    "MONITOR START"
                );
            }
            else if (result == 1)
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 011 MONITOR_ALREADY_RUNNING SID:%s\n",
                    SID
                );
            }
            else
            {
                snprintf(
                    response,
                    sizeof(response),
                    "ERR 012 MONITOR_FAILED SID:%s\n",
                    SID
                );
            }

            send_all(
                client_fd,
                response,
                strlen(response)
            );

            continue;
        }


        /*
         * ====================================================
         * MONITOR STOP
         * ====================================================
         */
        if (
            strcmp(
                buffer,
                "MONITOR STOP"
            ) == 0
        )
        {
            stop_monitor();

            char response[BUFFER_SIZE];

            snprintf(
                response,
                sizeof(response),
                "OK MONITOR_STOPPED SID:%s\n",
                SID
            );

            send_all(
                client_fd,
                response,
                strlen(response)
            );

            log_event(
                "MONITOR STOP"
            );

            continue;
        }


        /*
         * ====================================================
         * SYSINFO
         * ====================================================
         */
        if (
            strcmp(
                buffer,
                "SYSINFO"
            ) == 0
        )
        {
            double cpu =
                get_cpu_load();

            int memory =
                get_memory_usage();

            long uptime_seconds =
                get_uptime();

            char uptime_text[32];

            char response[BUFFER_SIZE];

            if (
                cpu < 0 ||
                memory < 0 ||
                uptime_seconds < 0
            )
            {
                send_error(
                    client_fd,
                    "ERR 014 SYSINFO_FAILED\n"
                );

                log_event(
                    "SYSINFO failed"
                );

                continue;
            }

            format_uptime(
                uptime_seconds,
                uptime_text,
                sizeof(uptime_text)
            );

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

            send_all(
                client_fd,
                response,
                strlen(response)
            );

            log_event(
                "SYSINFO requested"
            );

            continue;
        }


        /*
         * ====================================================
         * LISTPROC
         * ====================================================
         */
        if (
            strcmp(
                buffer,
                "LISTPROC"
            ) == 0
        )
        {
            handle_listproc(
                client_fd
            );

            continue;
        }


        /*
         * ====================================================
         * EXEC
         * ====================================================
         */
        if (
            strncmp(
                buffer,
                "EXEC ",
                5
            ) == 0
        )
        {
            if (
                strlen(buffer) <= 5
            )
            {
                send_error(
                    client_fd,
                    "ERR 002 COMMAND_NOT_ALLOWED\n"
                );

                continue;
            }

            handle_exec(
                client_fd,
                buffer
            );

            continue;
        }


        /*
         * ====================================================
         * PUT
         * ====================================================
         */
        if (
            strncmp(
                buffer,
                "PUT ",
                4
            ) == 0
        )
        {
            handle_put(
                client_fd,
                buffer
            );

            continue;
        }


        /*
         * ====================================================
         * GET
         * ====================================================
         */
        if (
            strncmp(
                buffer,
                "GET ",
                4
            ) == 0
        )
        {
            handle_get(
                client_fd,
                buffer
            );

            continue;
        }


        /*
         * ====================================================
         * UNKNOWN COMMAND
         * ====================================================
         */
        send_error(
            client_fd,
            "ERR 005 UNKNOWN_COMMAND\n"
        );

        log_event(
            "Unknown command received"
        );
    }

    close(client_fd);

    printf(
        "Client thread finished.\n"
    );

    return NULL;
}


/*
 * ============================================================
 * MAIN
 * ============================================================
 */
int main()
{
    /*
     * Prevent process termination when sending to a
     * disconnected TCP client.
     */
    signal(
        SIGPIPE,
        SIG_IGN
    );

    int server_fd;

    struct sockaddr_in server_addr;

    server_fd =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (
        server_fd < 0
    )
    {
        perror("socket");

        return 1;
    }

    printf(
        "Socket created successfully.\n"
    );

    int opt = 1;

    if (
        setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)
        ) < 0
    )
    {
        perror(
            "setsockopt"
        );

        close(server_fd);

        return 1;
    }

    if (
        ensure_storage_directory() < 0
    )
    {
        fprintf(
            stderr,
            "Warning: Could not create %s: %s\n",
            STORAGE_DIR,
            strerror(errno)
        );
    }

    memset(
        &server_addr,
        0,
        sizeof(server_addr)
    );

    server_addr.sin_family =
        AF_INET;

    server_addr.sin_addr.s_addr =
        INADDR_ANY;

    server_addr.sin_port =
        htons(PORT);

    if (
        bind(
            server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        ) < 0
    )
    {
        perror("bind");

        close(server_fd);

        return 1;
    }

    printf(
        "Server bound to port %d.\n",
        PORT
    );

    if (
        listen(
            server_fd,
            5
        ) < 0
    )
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
    printf("SYSINFO            : Enabled\n");
    printf("LISTPROC           : Enabled\n");
    printf("EXEC               : Enabled\n");
    printf("PUT                : Enabled\n");
    printf("GET                : Enabled\n");
    printf("UDP Monitoring     : Enabled\n");
    printf("Error Handling     : Enabled\n");
    printf("Storage            : %s\n", STORAGE_DIR);
    printf("=================================\n\n");

    log_event(
        "Agent started"
    );

    while (1)
    {
        struct sockaddr_in client_addr;

        socklen_t client_len =
            sizeof(client_addr);

        int client_fd =
            accept(
                server_fd,
                (struct sockaddr *)&client_addr,
                &client_len
            );

        if (
            client_fd < 0
        )
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror(
                "accept"
            );

            log_event(
                "accept failed"
            );

            continue;
        }

        printf(
            "New Controller connected.\n"
        );

        if (
            inet_ntop(
                AF_INET,
                &client_addr.sin_addr,
                connected_controller_ip,
                sizeof(connected_controller_ip)
            ) == NULL
        )
        {
            strncpy(
                connected_controller_ip,
                "127.0.0.1",
                sizeof(connected_controller_ip)
            );

            connected_controller_ip[
                sizeof(connected_controller_ip) - 1
            ] = '\0';
        }

        int *client_socket =
            malloc(sizeof(int));

        if (
            client_socket == NULL
        )
        {
            perror("malloc");

            send_error(
                client_fd,
                "ERR 015 SERVER_RESOURCE_ERROR\n"
            );

            close(client_fd);

            continue;
        }

        *client_socket =
            client_fd;

        pthread_t thread;

        int thread_result =
            pthread_create(
                &thread,
                NULL,
                handle_client,
                client_socket
            );

        if (
            thread_result != 0
        )
        {
            fprintf(
                stderr,
                "pthread_create failed: %s\n",
                strerror(thread_result)
            );

            free(client_socket);

            close(client_fd);

            log_event(
                "Client thread creation failed"
            );

            continue;
        }

        if (
            pthread_detach(thread) != 0
        )
        {
            log_event(
                "pthread_detach failed"
            );
        }
    }

    close(server_fd);

    return 0;
}
