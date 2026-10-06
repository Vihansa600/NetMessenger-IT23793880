#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define PORT 9880
#define BUFFER_SIZE 1024
#define NID "7938"

static int sockfd;
static volatile int running = 1;

/* Receive messages from the server */
void *receive_messages(void *arg)
{
    char buffer[BUFFER_SIZE];

    (void)arg;

    while (running)
    {
        int n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);

        if (n <= 0)
        {
            running = 0;
            break;
        }

        buffer[n] = '\0';

        printf("\nSERVER: %s", buffer);
        printf("> ");
        fflush(stdout);
    }

    return NULL;
}

/* Send a normal command */
int send_command(const char *command)
{
    char message[BUFFER_SIZE];

    snprintf(message, sizeof(message), "%s\n", command);

    if (send(sockfd, message, strlen(message), 0) < 0)
    {
        perror("send");
        return -1;
    }

    return 0;
}

/* Send a file */
int send_file(const char *target, const char *filename)
{
    FILE *fp;
    struct stat file_info;
    char header[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    if (stat(filename, &file_info) != 0)
    {
        perror("File not found");
        return -1;
    }

    fp = fopen(filename, "rb");

    if (fp == NULL)
    {
        perror("fopen");
        return -1;
    }

    /*
     * Assignment format:
     * SENDFILE <target> <filename> <filesize>
     */
    snprintf(
        header,
        sizeof(header),
        "SENDFILE %s %s %ld\n",
        target,
        filename,
        (long)file_info.st_size
    );

    if (send(sockfd, header, strlen(header), 0) < 0)
    {
        perror("send");
        fclose(fp);
        return -1;
    }

    /* Send exactly the file bytes */
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
        size_t total_sent = 0;

        while (total_sent < bytes_read)
        {
            ssize_t sent = send(
                sockfd,
                buffer + total_sent,
                bytes_read - total_sent,
                0
            );

            if (sent <= 0)
            {
                perror("send file");
                fclose(fp);
                return -1;
            }

            total_sent += sent;
        }
    }

    fclose(fp);

    printf("File sent: %s\n", filename);

    return 0;
}

int main(void)
{
    struct sockaddr_in server_addr;
    pthread_t receiver_thread;
    char command[BUFFER_SIZE];

    printf("=====================================\n");
    printf("       NetMessenger Client\n");
    printf("       Port : %d\n", PORT);
    printf("       NID  : %s\n", NID);
    printf("=====================================\n");

    /* Create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1",
                  &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sockfd);
        return 1;
    }

    /* Connect to server */
    if (connect(sockfd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("Connected to NetMessenger server.\n\n");

    printf("Available commands:\n");
    printf("REGISTER <username>\n");
    printf("LIST\n");
    printf("BCAST <message>\n");
    printf("PMSG <username> <message>\n");
    printf("JOIN <room>\n");
    printf("LEAVE <room>\n");
    printf("ROOMS\n");
    printf("RMSG <room> <message>\n");
    printf("SENDFILE <target> <filename>\n");
    printf("QUIT\n\n");

    /* Thread for receiving server messages */
    if (pthread_create(&receiver_thread,
                       NULL,
                       receive_messages,
                       NULL) != 0)
    {
        perror("pthread_create");
        close(sockfd);
        return 1;
    }

    while (running)
    {
        printf("> ");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            break;
        }

        command[strcspn(command, "\n")] = '\0';

        if (strlen(command) == 0)
        {
            continue;
        }

        /*
         * SENDFILE is special because the client
         * must calculate and send the file size.
         */
        if (strncmp(command, "SENDFILE ", 9) == 0)
        {
            char target[256];
            char filename[512];

            if (sscanf(command + 9,
                       "%255s %511s",
                       target,
                       filename) != 2)
            {
                printf("Usage: SENDFILE <target> <filename>\n");
                continue;
            }

            send_file(target, filename);
        }
        else
        {
            if (send_command(command) < 0)
            {
                running = 0;
                break;
            }

            if (strcmp(command, "QUIT") == 0)
            {
                running = 0;
                break;
            }
        }
    }

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    pthread_join(receiver_thread, NULL);

    printf("\nClient closed.\n");

    return 0;
}
