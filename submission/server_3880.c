#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>

#define PORT 9880
#define MAX_CLIENTS 20
#define MAX_ROOMS 20
#define MAX_ROOM_MEMBERS 20
#define BUFFER_SIZE 1024
#define NID "7938"

typedef struct
{
    int socket;
    char username[50];
    int registered;
} Client;

typedef struct
{
    char name[50];
    int used;
    int members[MAX_CLIENTS];
} Room;

Client clients[MAX_CLIENTS];
Room rooms[MAX_ROOMS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;


/* Send a normal response with NID */
void send_response(int socket, const char *message)
{
    char response[BUFFER_SIZE];

    snprintf(response,
             sizeof(response),
             "%s NID:%s\n",
             message,
             NID);

    send(socket,
         response,
         strlen(response),
         0);
}


/* Write event to log file */
void log_event(const char *message)
{
    FILE *file;

    file = fopen("netmsg_IT23793880.log", "a");

    if (file == NULL)
        return;

    time_t now = time(NULL);

    fprintf(file,
            "[%s] %s\n",
            ctime(&now),
            message);

    fclose(file);
}


/* Find client index using socket */
int find_client_by_socket(int socket)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].socket == socket)
            return i;
    }

    return -1;
}


/* Find client index using username */
int find_client_by_username(const char *username)
{
    int i;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].registered &&
            strcmp(clients[i].username, username) == 0)
        {
            return i;
        }
    }

    return -1;
}


/* Find room index using room name */
int find_room(const char *room_name)
{
    int i;

    for (i = 0; i < MAX_ROOMS; i++)
    {
        if (rooms[i].used &&
            strcmp(rooms[i].name, room_name) == 0)
        {
            return i;
        }
    }

    return -1;
}


/* Send broadcast message to all registered clients */
void broadcast_message(const char *message,
                       int sender_socket)
{
    int i;

    pthread_mutex_lock(&clients_mutex);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].registered &&
            clients[i].socket != sender_socket)
        {
            send(clients[i].socket,
                 message,
                 strlen(message),
                 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}


/* Remove client from all rooms */
void remove_client_from_rooms(int client_index)
{
    int i;

    pthread_mutex_lock(&rooms_mutex);

    for (i = 0; i < MAX_ROOMS; i++)
    {
        if (rooms[i].used)
        {
            rooms[i].members[client_index] = 0;
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
}


/* Remove disconnected client */
void remove_client(int socket)
{
    int index;
    char username[50];

    pthread_mutex_lock(&clients_mutex);

    index = find_client_by_socket(socket);

    if (index >= 0)
    {
        strcpy(username, clients[index].username);

        if (clients[index].registered)
        {
            char log_message[200];

            snprintf(log_message,
                     sizeof(log_message),
                     "Client disconnected: %s",
                     username);

            log_event(log_message);
        }

        clients[index].socket = 0;
        clients[index].username[0] = '\0';
        clients[index].registered = 0;
    }

    pthread_mutex_unlock(&clients_mutex);

    if (index >= 0)
    {
        remove_client_from_rooms(index);
    }
}

int recv_line(int socket, char *buffer, int size) 
{ 
    int position = 0; 
 
    while (position < size - 1) 
    { 
        char c; 
 
        int result = recv(socket, &c, 1, 0); 
 
        if (result <= 0) 
            return result; 
 
        buffer[position++] = c; 
 
        if (c == '\n') 
            break; 
    } 
 
    buffer[position] = '\0'; 
 
    return position; 
} 
 
int recv_file_data(int socket, FILE *file, long long filesize) 
{ 
    char buffer[4096]; 
    long long total = 0; 
 
    while (total < filesize) 
    { 
        long long remaining = filesize - total; 
 
        int chunk; 
 
        if (remaining > sizeof(buffer)) 
            chunk = sizeof(buffer); 
        else 
            chunk = (int)remaining; 
 
        int received = recv(socket, 
                            buffer, 
                            chunk, 
                            0); 
 
        if (received <= 0) 
            return -1; 
 
        fwrite(buffer, 1, received, file); 
 
        total += received; 
    } 
 
    return 0; 
}
/* Handle one connected client */
void *client_handler(void *arg)
{
    int client_socket;
    char buffer[BUFFER_SIZE];

    client_socket = *(int *)arg;

    free(arg);

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));

        int bytes = recv_line(client_socket,
                         buffer,
                         sizeof(buffer));

        if (bytes <= 0)
        {
            remove_client(client_socket);
            close(client_socket);
            return NULL;
        }

        buffer[bytes] = '\0';

        buffer[strcspn(buffer, "\r\n")] = '\0';


        /* REGISTER */
        if (strncmp(buffer, "REGISTER ", 9) == 0)
        {
            char username[50];

            if (sscanf(buffer + 9,
                       "%49s",
                       username) != 1)
            {
                send_response(client_socket,
                              "ERR 006 INVALID_COMMAND");
                continue;
            }

            pthread_mutex_lock(&clients_mutex);

            int username_taken =
                find_client_by_username(username);

            int client_index =
                find_client_by_socket(client_socket);

            if (username_taken >= 0)
            {
                pthread_mutex_unlock(&clients_mutex);

                send_response(client_socket,
                              "ERR 001 USERNAME_TAKEN");

                continue;
            }

            if (client_index >= 0)
            {
                strcpy(clients[client_index].username,
                       username);

                clients[client_index].registered = 1;
            }

            pthread_mutex_unlock(&clients_mutex);

            char response[100];

            snprintf(response,
                     sizeof(response),
                     "OK REGISTERED %s",
                     username);

            send_response(client_socket,
                          response);

            char log_message[200];

            snprintf(log_message,
                     sizeof(log_message),
                     "User registered: %s",
                     username);

            log_event(log_message);

            char notification[150];

            snprintf(notification,
                     sizeof(notification),
                     "MSG SYSTEM %s joined the server.\n",
                     username);

            broadcast_message(notification,
                              client_socket);

            continue;
        }


        /* LIST */
        if (strcmp(buffer, "LIST") == 0)
        {
            char users[BUFFER_SIZE];

            strcpy(users, "OK USERS ");

            pthread_mutex_lock(&clients_mutex);

            int first = 1;
            int i;

            for (i = 0; i < MAX_CLIENTS; i++)
            {
                if (clients[i].registered)
                {
                    if (!first)
                        strcat(users, ",");

                    strcat(users,
                           clients[i].username);

                    first = 0;
                }
            }

            pthread_mutex_unlock(&clients_mutex);

            send_response(client_socket,
                          users);

            continue;
        }


        /* BCAST */
        if (strncmp(buffer, "BCAST ", 6) == 0)
        {
            char sender[50] = "";

            pthread_mutex_lock(&clients_mutex);

            int sender_index =
                find_client_by_socket(client_socket);

            if (sender_index >= 0 &&
                clients[sender_index].registered)
            {
                strcpy(sender,
                       clients[sender_index].username);
            }

            pthread_mutex_unlock(&clients_mutex);

            if (strlen(sender) == 0)
            {
                send_response(client_socket,
                              "ERR 005 NOT_REGISTERED");
                continue;
            }

            char message[BUFFER_SIZE];

            snprintf(message,
                     sizeof(message),
                     "MSG BCAST %s %s\n",
                     sender,
                     buffer + 6);

            broadcast_message(message,
                              client_socket);

            send_response(client_socket,
                          "OK SENT");

            log_event("Broadcast message sent");

            continue;
        }


        /* PMSG */
        if (strncmp(buffer, "PMSG ", 5) == 0)
        {
            char target[50];
            char message[BUFFER_SIZE];

            if (sscanf(buffer + 5,
                       "%49s %[^\n]",
                       target,
                       message) < 2)
            {
                send_response(client_socket,
                              "ERR 002 USER_NOT_FOUND");
                continue;
            }

            pthread_mutex_lock(&clients_mutex);

            int sender_index =
                find_client_by_socket(client_socket);

            int target_index =
                find_client_by_username(target);

            if (sender_index < 0 ||
                !clients[sender_index].registered)
            {
                pthread_mutex_unlock(&clients_mutex);

                send_response(client_socket,
                              "ERR 005 NOT_REGISTERED");

                continue;
            }

            if (target_index < 0)
            {
                pthread_mutex_unlock(&clients_mutex);

                send_response(client_socket,
                              "ERR 002 USER_NOT_FOUND");

                continue;
            }

            char private_message[BUFFER_SIZE];

            snprintf(private_message,
                     sizeof(private_message),
                     "MSG PRIV %s %s\n",
                     clients[sender_index].username,
                     message);

            send(clients[target_index].socket,
                 private_message,
                 strlen(private_message),
                 0);

            pthread_mutex_unlock(&clients_mutex);

            send_response(client_socket,
                          "OK SENT");

            log_event("Private message sent");

            continue;
        }


        /* JOIN */
        if (strncmp(buffer, "JOIN ", 5) == 0)
        {
            char room_name[50];

            if (sscanf(buffer + 5,
                       "%49s",
                       room_name) != 1)
            {
                send_response(client_socket,
                              "ERR 003 ROOM_NOT_FOUND");
                continue;
            }

            pthread_mutex_lock(&clients_mutex);

            int client_index =
                find_client_by_socket(client_socket);

            int registered =
                (client_index >= 0 &&
                 clients[client_index].registered);

            pthread_mutex_unlock(&clients_mutex);

            if (!registered)
            {
                send_response(client_socket,
                              "ERR 005 NOT_REGISTERED");
                continue;
            }

            pthread_mutex_lock(&rooms_mutex);

            int room_index =
                find_room(room_name);

            /* Create room if it does not exist */
            if (room_index < 0)
            {
                int i;

                for (i = 0; i < MAX_ROOMS; i++)
                {
                    if (!rooms[i].used)
                    {
                        room_index = i;

                        rooms[i].used = 1;

                        strcpy(rooms[i].name,
                               room_name);

                        memset(rooms[i].members,
                               0,
                               sizeof(rooms[i].members));

                        break;
                    }
                }
            }

            if (room_index < 0)
            {
                pthread_mutex_unlock(&rooms_mutex);

                send_response(client_socket,
                              "ERR 004 ROOM_FULL");

                continue;
            }

            rooms[room_index].members[client_index] = 1;

            pthread_mutex_unlock(&rooms_mutex);

            char response[100];

            snprintf(response,
                     sizeof(response),
                     "OK JOINED %s",
                     room_name);

            send_response(client_socket,
                          response);

            char log_message[200];

            snprintf(log_message,
                     sizeof(log_message),
                     "User joined room: %s",
                     room_name);

            log_event(log_message);

            continue;
        }


        /* LEAVE */
        if (strncmp(buffer, "LEAVE ", 6) == 0)
        {
            char room_name[50];

            if (sscanf(buffer + 6,
                       "%49s",
                       room_name) != 1)
            {
                send_response(client_socket,
                              "ERR 003 ROOM_NOT_FOUND");
                continue;
            }

            pthread_mutex_lock(&clients_mutex);

            int client_index =
                find_client_by_socket(client_socket);

            pthread_mutex_unlock(&clients_mutex);

            pthread_mutex_lock(&rooms_mutex);

            int room_index =
                find_room(room_name);

            if (room_index < 0)
            {
                pthread_mutex_unlock(&rooms_mutex);

                send_response(client_socket,
                              "ERR 003 ROOM_NOT_FOUND");

                continue;
            }

            rooms[room_index].members[client_index] = 0;

            pthread_mutex_unlock(&rooms_mutex);

            char response[100];

            snprintf(response,
                     sizeof(response),
                     "OK LEFT %s",
                     room_name);

            send_response(client_socket,
                          response);

            continue;
        }


        /* ROOMS */
        if (strcmp(buffer, "ROOMS") == 0)
        {
            char room_list[BUFFER_SIZE];

            strcpy(room_list,
                   "OK ROOMS ");

            pthread_mutex_lock(&rooms_mutex);

            int first = 1;
            int i;

            for (i = 0; i < MAX_ROOMS; i++)
            {
                if (rooms[i].used)
                {
                    if (!first)
                        strcat(room_list, ",");

                    strcat(room_list,
                           rooms[i].name);

                    first = 0;
                }
            }

            pthread_mutex_unlock(&rooms_mutex);

            send_response(client_socket,
                          room_list);

            continue;
        }


        /* RMSG */
        if (strncmp(buffer, "RMSG ", 5) == 0)
        {
            char room_name[50];
            char message[BUFFER_SIZE];

            if (sscanf(buffer + 5,
                       "%49s %[^\n]",
                       room_name,
                       message) < 2)
            {
                send_response(client_socket,
                              "ERR 003 ROOM_NOT_FOUND");
                continue;
            }

            pthread_mutex_lock(&clients_mutex);

            int sender_index =
                find_client_by_socket(client_socket);

            if (sender_index < 0 ||
                !clients[sender_index].registered)
            {
                pthread_mutex_unlock(&clients_mutex);

                send_response(client_socket,
                              "ERR 005 NOT_REGISTERED");

                continue;
            }

            char sender[50];

            strcpy(sender,
                   clients[sender_index].username);

            pthread_mutex_unlock(&clients_mutex);

            pthread_mutex_lock(&rooms_mutex);

            int room_index =
                find_room(room_name);

            if (room_index < 0)
            {
                pthread_mutex_unlock(&rooms_mutex);

                send_response(client_socket,
                              "ERR 003 ROOM_NOT_FOUND");

                continue;
            }

            if (!rooms[room_index].members[sender_index])
            {
                pthread_mutex_unlock(&rooms_mutex);

                send_response(client_socket,
                              "ERR 003 ROOM_NOT_FOUND");

                continue;
            }

            char room_message[BUFFER_SIZE];

            snprintf(room_message,
                     sizeof(room_message),
                     "MSG ROOM %s %s %s\n",
                     room_name,
                     sender,
                     message);

            int i;

            for (i = 0; i < MAX_CLIENTS; i++)
            {
                if (rooms[room_index].members[i] &&
                    clients[i].registered)
                {
                    send(clients[i].socket,
                         room_message,
                         strlen(room_message),
                         0);
                }
            }

            pthread_mutex_unlock(&rooms_mutex);

            send_response(client_socket,
                          "OK SENT");

            log_event("Room message sent");

            continue;
        }

/* SENDFILE */ 
if (strncmp(buffer, "SENDFILE ", 9) == 0) 
{ 
    char target[50]; 
    char filename[200]; 
    long long filesize; 
 
    if (sscanf(buffer + 9, 
               "%49s %199s %lld", 
               target, 
               filename, 
               &filesize) != 3) 
    { 
        send_response(client_socket, 
                      "ERR 006 INVALID_COMMAND"); 
        continue; 
    } 
 
    if (filesize < 0) 
    { 
        send_response(client_socket, 
                      "ERR 006 INVALID_COMMAND"); 
        continue; 
    } 
 
    if (strstr(filename, "..") != NULL || 
        strchr(filename, '/') != NULL || 
        strchr(filename, '\\') != NULL) 
    { 
        send_response(client_socket, 
                      "ERR 006 INVALID_COMMAND"); 
        continue; 
    } 
 
    pthread_mutex_lock(&clients_mutex); 
 
    int sender_index = 
        find_client_by_socket(client_socket); 
 
    int target_index = 
        find_client_by_username(target); 
 
    if (sender_index < 0 || 
        !clients[sender_index].registered) 
    { 
        pthread_mutex_unlock(&clients_mutex); 
 
        send_response(client_socket, 
                      "ERR 005 NOT_REGISTERED"); 
 
        continue; 
    } 
 
    if (target_index < 0) 
    { 
        pthread_mutex_unlock(&clients_mutex); 
 
        send_response(client_socket, 
                      "ERR 002 USER_NOT_FOUND"); 
 
        continue; 
    } 
 
    char sender[50]; 
 
    strcpy(sender, 
           clients[sender_index].username); 
 
    pthread_mutex_unlock(&clients_mutex); 
 
    mkdir("storage", 0755); 
    mkdir("storage/IT23793880", 0755); 
 
    char user_directory[300]; 
 
    snprintf(user_directory, 
             sizeof(user_directory), 
             "storage/IT23793880/%s", 
             sender); 
 
    mkdir(user_directory, 0755); 
 
    char file_path[500]; 
 
    snprintf(file_path, 
             sizeof(file_path), 
             "%s/%s", 
             user_directory, 
             filename); 
 
    FILE *file = fopen(file_path, "wb"); 
 
    if (file == NULL) 
    { 
        send_response(client_socket, 
                      "ERR 006 INVALID_COMMAND"); 
        continue; 
    } 
 
    if (recv_file_data(client_socket, 
                       file, 
                       filesize) < 0) 
    { 
        fclose(file); 
        remove(file_path); 
 
        send_response(client_socket, 
                      "ERR 006 INVALID_COMMAND"); 
 
        continue; 
    } 
 
    fclose(file); 
 
    char log_message[500]; 
 
    snprintf(log_message, 
             sizeof(log_message), 
             "File received: sender=%s target=%s filename=%s size=%lld", 
             sender, 
             target, 
             filename, 
             filesize); 
 
    log_event(log_message); 
 
    char response[300]; 
 
    snprintf(response, 
             sizeof(response), 
             "OK FILE_RECEIVED %s", 
             filename); 
 
    send_response(client_socket, 
                  response); 
 
    continue; 
}
        /* QUIT */
        if (strcmp(buffer, "QUIT") == 0)
        {
            send_response(client_socket,
                          "OK BYE");

            remove_client(client_socket);

            close(client_socket);

            return NULL;
        }


        /* Invalid command */
        send_response(client_socket,
                      "ERR 006 INVALID_COMMAND");
    }

    return NULL;
}


/* Main server */
int main()
{
    int server_socket;

    struct sockaddr_in server_address;

    server_socket = socket(AF_INET,
                           SOCK_STREAM,
                           0);

    if (server_socket < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int option = 1;

    setsockopt(server_socket,
               SOL_SOCKET,
               SO_REUSEADDR,
               &option,
               sizeof(option));

    memset(&server_address,
           0,
           sizeof(server_address));

    server_address.sin_family = AF_INET;

    server_address.sin_addr.s_addr =
        INADDR_ANY;

    server_address.sin_port =
        htons(PORT);


    if (bind(server_socket,
             (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0)
    {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }


    if (listen(server_socket, 10) < 0)
    {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }


    printf("====================================\n");
    printf("       NetMessenger Server\n");
    printf("====================================\n");
    printf("Registration : IT23793880\n");
    printf("NID          : %s\n", NID);
    printf("Port         : %d\n", PORT);
    printf("Server ready...\n");
    printf("====================================\n");


    log_event("Server started");


    while (1)
    {
        struct sockaddr_in client_address;

        socklen_t client_length =
            sizeof(client_address);

        int client_socket =
            accept(server_socket,
                   (struct sockaddr *)&client_address,
                   &client_length);

        if (client_socket < 0)
        {
            perror("Accept failed");
            continue;
        }


        int added = 0;

        pthread_mutex_lock(&clients_mutex);

        int i;

        for (i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].socket == 0)
            {
                clients[i].socket =
                    client_socket;

                clients[i].registered = 0;

                clients[i].username[0] =
                    '\0';

                added = 1;

                break;
            }
        }

        pthread_mutex_unlock(&clients_mutex);


        if (!added)
        {
            send_response(client_socket,
                          "ERR 007 SERVER_FULL");

            close(client_socket);

            continue;
        }


        int *socket_pointer =
            malloc(sizeof(int));

        if (socket_pointer == NULL)
        {
            close(client_socket);
            continue;
        }

        *socket_pointer =
            client_socket;


        pthread_t thread;

        pthread_create(&thread,
                       NULL,
                       client_handler,
                       socket_pointer);

        pthread_detach(thread);


        printf("New client connected.\n");
    }


    close(server_socket);

    return 0;
}
