#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>

#define PORT 9410
#define BUFFER_SIZE 1024

#define DOWNLOAD_PREFIX "received_"

#define UDP_PORT 9410

int udp_socket_fd = -1;
volatile int udp_receiver_running = 1;

void *udp_receiver(void *arg)
{
    (void)arg;

    char buffer[BUFFER_SIZE];
    struct sockaddr_in agent_addr;
    socklen_t agent_len;

    while (udp_receiver_running)
    {
        fd_set read_fds;
        struct timeval timeout;

        FD_ZERO(&read_fds);
        FD_SET(udp_socket_fd, &read_fds);

        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        int result = select(
            udp_socket_fd + 1,
            &read_fds,
            NULL,
            NULL,
            &timeout
        );

        if (!udp_receiver_running)
        {
            break;
        }

        if (result <= 0)
        {
            continue;
        }

        agent_len = sizeof(agent_addr);

        ssize_t bytes_received = recvfrom(
            udp_socket_fd,
            buffer,
            sizeof(buffer) - 1,
            0,
            (struct sockaddr *)&agent_addr,
            &agent_len
        );

        if (bytes_received < 0)
        {
            continue;
        }

        buffer[bytes_received] = '\0';

        printf(
            "\nUDP Monitor [%s:%d] -> %s\n",
            inet_ntoa(agent_addr.sin_addr),
            ntohs(agent_addr.sin_port),
            buffer
        );

        printf("\nRemoteOps> ");
        fflush(stdout);
    }

    return NULL;
}

int start_udp_receiver()
{
    struct sockaddr_in udp_addr;
    int opt = 1;

    udp_socket_fd = socket(
        AF_INET,
        SOCK_DGRAM,
        0
    );

    if (udp_socket_fd < 0)
    {
        perror("UDP socket");
        return -1;
    }

    setsockopt(
        udp_socket_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt,
        sizeof(opt)
    );

    memset(
        &udp_addr,
        0,
        sizeof(udp_addr)
    );

    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(UDP_PORT);

    if (bind(
        udp_socket_fd,
        (struct sockaddr *)&udp_addr,
        sizeof(udp_addr)
    ) < 0)
    {
        perror("UDP bind");
        close(udp_socket_fd);
        udp_socket_fd = -1;
        return -1;
    }

    pthread_t thread;
    udp_receiver_running = 1;

    if (pthread_create(
        &thread,
        NULL,
        udp_receiver,
        NULL
    ) != 0)
    {
        perror("pthread_create");
        close(udp_socket_fd);
        udp_socket_fd = -1;
        return -1;
    }

    pthread_detach(thread);

    printf(
        "UDP monitor receiver listening on port %d.\n",
        UDP_PORT
    );

    return 0;
}

void stop_udp_receiver()
{
    udp_receiver_running = 0;

    if (udp_socket_fd >= 0)
    {
        close(udp_socket_fd);
        udp_socket_fd = -1;
    }
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


    while (
        total_sent < total_bytes
    )
    {
        ssize_t bytes_sent =
            send(
                socket_fd,
                buffer + total_sent,
                total_bytes - total_sent,
                0
            );


        if (
            bytes_sent <= 0
        )
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


    while (
        total_received < total_bytes
    )
    {
        ssize_t bytes_received =
            recv(
                socket_fd,
                buffer + total_received,
                total_bytes - total_received,
                0
            );


        if (
            bytes_received <= 0
        )
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
uint64_t htonll(
    uint64_t value
)
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


uint64_t ntohll(
    uint64_t value
)
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
 * Receives one complete text line.
 *
 * Example:
 *
 * OK AUTHENTICATED SID:8652
 *
 * The '\n' is consumed but not included in the final string.
 */
int recv_line(
    int socket_fd,
    char *buffer,
    size_t buffer_size
)
{
    size_t index = 0;


    if (
        buffer == NULL ||
        buffer_size < 2
    )
    {
        return -1;
    }


    while (
        index < buffer_size - 1
    )
    {
        char character;


        ssize_t result =
            recv(
                socket_fd,
                &character,
                1,
                0
            );


        if (
            result <= 0
        )
        {
            return -1;
        }


        /*
         * Stop at newline.
         */
        if (
            character == '\n'
        )
        {
            break;
        }


        /*
         * Ignore carriage return.
         */
        if (
            character == '\r'
        )
        {
            continue;
        }


        buffer[index++] =
            character;
    }


    buffer[index] =
        '\0';


    return (int)index;
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


    /*
     * Prevent path traversal.
     */
    if (
        strchr(
            filename,
            '/'
        ) != NULL ||
        strchr(
            filename,
            '\\'
        ) != NULL
    )
    {
        return 0;
    }


    if (
        strcmp(
            filename,
            "."
        ) == 0 ||
        strcmp(
            filename,
            ".."
        ) == 0
    )
    {
        return 0;
    }


    return 1;
}


/*
 * ============================================================
 * EXTRACT FILENAME
 * ============================================================
 */
int extract_filename(
    const char *command,
    const char *prefix,
    char *filename,
    size_t filename_size
)
{
    const char *argument;


    if (
        command == NULL ||
        prefix == NULL ||
        filename == NULL
    )
    {
        return 0;
    }


    argument =
        command + strlen(prefix);


    /*
     * Extract first word after PUT / GET.
     */
    if (
        sscanf(
            argument,
            "%255s",
            filename
        ) != 1
    )
    {
        return 0;
    }


    if (
        strlen(filename) == 0 ||
        strlen(filename) >= filename_size
    )
    {
        return 0;
    }


    if (
        !valid_filename(filename)
    )
    {
        return 0;
    }


    return 1;
}


/*
 * ============================================================
 * PUT - FILE UPLOAD
 * ============================================================
 */
int handle_put(
    int client_fd,
    const char *command
)
{
    char filename[256];

    char response[BUFFER_SIZE];


    /*
     * Extract filename.
     */
    if (
        !extract_filename(
            command,
            "PUT ",
            filename,
            sizeof(filename)
        )
    )
    {
        printf(
            "Usage: PUT <filename>\n"
        );

        return 0;
    }


    /*
     * Open local file.
     */
    FILE *input_file =
        fopen(
            filename,
            "rb"
        );


    if (
        input_file == NULL
    )
    {
        printf(
            "File not found: %s\n",
            filename
        );

        return 0;
    }


    /*
     * Find file size.
     */
    if (
        fseek(
            input_file,
            0,
            SEEK_END
        ) != 0
    )
    {
        fclose(
            input_file
        );

        printf(
            "Unable to determine file size.\n"
        );

        return 0;
    }


    long file_size_long =
        ftell(
            input_file
        );


    if (
        file_size_long < 0
    )
    {
        fclose(
            input_file
        );

        printf(
            "Unable to determine file size.\n"
        );

        return 0;
    }


    rewind(
        input_file
    );


    uint64_t file_size =
        (uint64_t)file_size_long;


    /*
     * Send PUT command.
     */
    char command_buffer[BUFFER_SIZE];


    int command_length =
        snprintf(
            command_buffer,
            sizeof(command_buffer),
            "PUT %s\n",
            filename
        );


    if (
        command_length < 0 ||
        (size_t)command_length >= sizeof(command_buffer)
    )
    {
        fclose(
            input_file
        );

        printf(
            "PUT command is too long.\n"
        );

        return 0;
    }


    if (
        send_all(
            client_fd,
            command_buffer,
            (size_t)command_length
        ) < 0
    )
    {
        fclose(
            input_file
        );

        return -1;
    }


    /*
     * Wait for Agent READY.
     */
    if (
        recv_line(
            client_fd,
            response,
            sizeof(response)
        ) < 0
    )
    {
        fclose(
            input_file
        );

        printf(
            "Agent disconnected.\n"
        );

        return -1;
    }


    printf(
        "Agent: %s\n",
        response
    );


    /*
     * Check READY response.
     */
    if (
        strncmp(
            response,
            "OK READY",
            8
        ) != 0
    )
    {
        fclose(
            input_file
        );

        printf(
            "PUT rejected by Agent.\n"
        );

        return 0;
    }


    /*
     * Send 8-byte file size.
     */
    uint64_t network_file_size =
        htonll(
            file_size
        );


    if (
        send_all(
            client_fd,
            &network_file_size,
            sizeof(network_file_size)
        ) < 0
    )
    {
        fclose(
            input_file
        );

        return -1;
    }


    /*
     * Send exact file bytes.
     */
    char file_buffer[BUFFER_SIZE];

    uint64_t remaining =
        file_size;


    while (
        remaining > 0
    )
    {
        size_t chunk_size;


        if (
            remaining > BUFFER_SIZE
        )
        {
            chunk_size =
                BUFFER_SIZE;
        }
        else
        {
            chunk_size =
                (size_t)remaining;
        }


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
            fclose(
                input_file
            );

            printf(
                "Error reading local file.\n"
            );

            return -1;
        }


        if (
            send_all(
                client_fd,
                file_buffer,
                bytes_read
            ) < 0
        )
        {
            fclose(
                input_file
            );

            return -1;
        }


        remaining -=
            (uint64_t)bytes_read;
    }


    fclose(
        input_file
    );


    /*
     * Receive final Agent response.
     */
    if (
        recv_line(
            client_fd,
            response,
            sizeof(response)
        ) < 0
    )
    {
        printf(
            "Agent disconnected.\n"
        );

        return -1;
    }


    printf(
        "Agent: %s\n",
        response
    );


    if (
        strcmp(
            response,
            "OK FILE_RECEIVED SID:8652"
        ) == 0
    )
    {
        printf(
            "PUT completed successfully.\n"
        );
    }
    else
    {
        printf(
            "PUT failed.\n"
        );
    }


    return 0;
}


/*
 * ============================================================
 * GET - FILE DOWNLOAD
 * ============================================================
 *
 * Protocol:
 *
 * Controller:
 *     GET test.txt
 *
 * Agent:
 *     OK FILE_READY SID:8652 SIZE:30
 *
 * Agent:
 *     [30 bytes]
 *
 * Agent:
 *     OK FILE_SENT SID:8652
 *
 * Controller:
 *     saves file as received_test.txt
 */
int handle_get(
    int client_fd,
    const char *command
)
{
    char filename[256];

    char response[BUFFER_SIZE];


    /*
     * Extract filename.
     */
    if (
        !extract_filename(
            command,
            "GET ",
            filename,
            sizeof(filename)
        )
    )
    {
        printf(
            "Usage: GET <filename>\n"
        );

        return 0;
    }


    /*
     * Send GET command.
     */
    char command_buffer[BUFFER_SIZE];


    int command_length =
        snprintf(
            command_buffer,
            sizeof(command_buffer),
            "GET %s\n",
            filename
        );


    if (
        command_length < 0 ||
        (size_t)command_length >= sizeof(command_buffer)
    )
    {
        printf(
            "GET command is too long.\n"
        );

        return 0;
    }


    if (
        send_all(
            client_fd,
            command_buffer,
            (size_t)command_length
        ) < 0
    )
    {
        return -1;
    }


    /*
     * Receive FILE_READY header.
     */
    if (
        recv_line(
            client_fd,
            response,
            sizeof(response)
        ) < 0
    )
    {
        printf(
            "Agent disconnected.\n"
        );

        return -1;
    }


    printf(
        "Agent: %s\n",
        response
    );


    /*
     * Check Agent response.
     */
    if (
        strncmp(
            response,
            "OK FILE_READY",
            strlen("OK FILE_READY")
        ) != 0
    )
    {
        printf(
            "GET failed.\n"
        );

        return 0;
    }


    /*
     * Extract file size.
     *
     * Example:
     *
     * OK FILE_READY SID:8652 SIZE:30
     */
    unsigned long long received_size;


    if (
        sscanf(
            response,
            "OK FILE_READY SID:%*s SIZE:%llu",
            &received_size
        ) != 1
    )
    {
        printf(
            "Invalid GET response.\n"
        );

        return -1;
    }


    uint64_t file_size =
        (uint64_t)received_size;


    /*
     * Create local filename.
     *
     * GET test.txt
     *
     * -> received_test.txt
     */
    char output_filename[512];


    int output_length =
        snprintf(
            output_filename,
            sizeof(output_filename),
            "%s%s",
            DOWNLOAD_PREFIX,
            filename
        );


    if (
        output_length < 0 ||
        (size_t)output_length >= sizeof(output_filename)
    )
    {
        printf(
            "Output filename is too long.\n"
        );

        return -1;
    }


    /*
     * Open local output file.
     */
    FILE *output_file =
        fopen(
            output_filename,
            "wb"
        );


    if (
        output_file == NULL
    )
    {
        perror(
            "fopen"
        );

        return -1;
    }


    /*
     * Receive EXACTLY file_size bytes.
     */
    char file_buffer[BUFFER_SIZE];

    uint64_t remaining =
        file_size;


    while (
        remaining > 0
    )
    {
        size_t chunk_size;


        if (
            remaining > BUFFER_SIZE
        )
        {
            chunk_size =
                BUFFER_SIZE;
        }
        else
        {
            chunk_size =
                (size_t)remaining;
        }


        ssize_t bytes_received =
            recv(
                client_fd,
                file_buffer,
                chunk_size,
                0
            );


        if (
            bytes_received <= 0
        )
        {
            fclose(
                output_file
            );

            remove(
                output_filename
            );

            printf(
                "GET: incomplete file transfer.\n"
            );

            return -1;
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
            fclose(
                output_file
            );

            remove(
                output_filename
            );

            printf(
                "GET: file write error.\n"
            );

            return -1;
        }


        remaining -=
            (uint64_t)bytes_received;
    }


    /*
     * File bytes are now completely received.
     */
    fclose(
        output_file
    );


    /*
     * IMPORTANT:
     *
     * Now receive Agent's final FILE_SENT response.
     *
     * The previous controller code was missing this step.
     */
    if (
        recv_line(
            client_fd,
            response,
            sizeof(response)
        ) < 0
    )
    {
        remove(
            output_filename
        );

        printf(
            "GET failed: final Agent response missing.\n"
        );

        return -1;
    }


    printf(
        "Agent: %s\n",
        response
    );


    /*
     * Verify final Agent acknowledgement.
     */
    if (
        strcmp(
            response,
            "OK FILE_SENT SID:8652"
        ) != 0
    )
    {
        remove(
            output_filename
        );

        printf(
            "GET failed: invalid final response.\n"
        );

        return 0;
    }


    /*
     * Successful GET.
     */
    printf(
        "GET completed successfully.\n"
    );


    printf(
        "Saved as: %s\n",
        output_filename
    );


    printf(
        "Received bytes: %llu\n",
        (unsigned long long)file_size
    );


    return 0;
}


/*
 * ============================================================
 * HANDLE LISTPROC
 * ============================================================
 *
 * Agent sends:
 *
 * OK PROCS SID:8652
 * process line
 * process line
 * ...
 * END PROCS
 */
int handle_listproc(
    int client_fd
)
{
    char response[BUFFER_SIZE];


    while (1)
    {
        if (
            recv_line(
                client_fd,
                response,
                sizeof(response)
            ) < 0
        )
        {
            printf(
                "Agent disconnected.\n"
            );

            return -1;
        }


        printf(
            "%s\n",
            response
        );


        if (
            strcmp(
                response,
                "END PROCS"
            ) == 0
        )
        {
            break;
        }
    }


    return 0;
}


/*
 * ============================================================
 * MAIN
 * ============================================================
 */
int main()
{
    int client_fd;

    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE];


    /*
     * ========================================================
     * CREATE SOCKET
     * ========================================================
     */
    client_fd =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );


    if (
        client_fd < 0
    )
    {
        perror(
            "socket"
        );

        return 1;
    }


    printf(
        "Socket created successfully.\n"
    );


    /*
     * ========================================================
     * CONFIGURE AGENT ADDRESS
     * ========================================================
     */
    memset(
        &server_addr,
        0,
        sizeof(server_addr)
    );


    server_addr.sin_family =
        AF_INET;


    server_addr.sin_port =
        htons(PORT);


    if (
        inet_pton(
            AF_INET,
            "127.0.0.1",
            &server_addr.sin_addr
        ) <= 0
    )
    {
        perror(
            "inet_pton"
        );

        close(
            client_fd
        );

        return 1;
    }


    /*
     * ========================================================
     * CONNECT TO AGENT
     * ========================================================
     */
    if (
        connect(
            client_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)
        ) < 0
    )
    {
        perror(
            "connect"
        );

        close(
            client_fd
        );

        return 1;
    }


    printf(
        "Connected to RemoteOps Agent.\n"
    );

    if (start_udp_receiver() < 0)
    {
        printf(
            "UDP monitoring is unavailable.\n"
        );
    }



    /*
     * ========================================================
     * COMMAND LOOP
     * ========================================================
     */
    while (1)
    {
        printf(
            "\nRemoteOps> "
        );


        fflush(
            stdout
        );


        if (
            fgets(
                buffer,
                sizeof(buffer),
                stdin
            ) == NULL
        )
        {
            break;
        }


        /*
         * Remove newline.
         */
        buffer[
            strcspn(
                buffer,
                "\r\n"
            )
        ] = '\0';


        /*
         * Ignore empty command.
         */
        if (
            strlen(buffer) == 0
        )
        {
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
            int result =
                handle_put(
                    client_fd,
                    buffer
                );


            if (
                result < 0
            )
            {
                break;
            }


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
            int result =
                handle_get(
                    client_fd,
                    buffer
                );


            if (
                result < 0
            )
            {
                break;
            }


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
            char command_line[] =
                "LISTPROC\n";


            if (
                send_all(
                    client_fd,
                    command_line,
                    strlen(command_line)
                ) < 0
            )
            {
                perror(
                    "send"
                );

                break;
            }


            if (
                handle_listproc(
                    client_fd
                ) < 0
            )
            {
                break;
            }


            continue;
        }


        if (
            strcmp(
                buffer,
                "MONITOR START"
            ) == 0 ||
            strcmp(
                buffer,
                "MONITOR STOP"
            ) == 0
        )
        {
            char command_to_send[BUFFER_SIZE];

            int command_length =
                snprintf(
                    command_to_send,
                    sizeof(command_to_send),
                    "%s\n",
                    buffer
                );

            if (
                command_length < 0 ||
                (size_t)command_length >= sizeof(command_to_send)
            )
            {
                printf(
                    "Command is too long.\n"
                );

                continue;
            }

            if (
                send_all(
                    client_fd,
                    command_to_send,
                    (size_t)command_length
                ) < 0
            )
            {
                perror(
                    "send"
                );

                break;
            }

            if (
                recv_line(
                    client_fd,
                    buffer,
                    sizeof(buffer)
                ) < 0
            )
            {
                printf(
                    "Agent disconnected.\n"
                );

                break;
            }

            printf(
                "Agent: %s\n",
                buffer
            );

            continue;
        }


        /*
         * ====================================================
         * NORMAL COMMANDS
         *
         * AUTH
         * SYSINFO
         * EXEC
         * QUIT
         * ====================================================
         */
        char command_to_send[BUFFER_SIZE];


        int command_length =
            snprintf(
                command_to_send,
                sizeof(command_to_send),
                "%s\n",
                buffer
            );


        if (
            command_length < 0 ||
            (size_t)command_length >=
                sizeof(command_to_send)
        )
        {
            printf(
                "Command is too long.\n"
            );

            continue;
        }


        if (
            send_all(
                client_fd,
                command_to_send,
                (size_t)command_length
            ) < 0
        )
        {
            perror(
                "send"
            );

            break;
        }


        /*
         * ====================================================
         * RECEIVE NORMAL RESPONSE
         * ====================================================
         */
        if (
            recv_line(
                client_fd,
                buffer,
                sizeof(buffer)
            ) < 0
        )
        {
            printf(
                "Agent disconnected.\n"
            );

            break;
        }


        printf(
            "Agent: %s\n",
            buffer
        );


        /*
         * ====================================================
         * QUIT
         * ====================================================
         */
        if (
            strcmp(
                buffer,
                "OK BYE SID:8652"
            ) == 0
        )
        {
            break;
        }
    }


    /*
     * ========================================================
     * CLOSE SOCKET
     * ========================================================
     */
    stop_udp_receiver();

    close(
        client_fd
    );


    printf(
        "Controller closed.\n"
    );


    return 0;
}
