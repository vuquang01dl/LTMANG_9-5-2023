#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/select.h>


#define PORT 8888
#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    int master_socket, new_socket, activity, addrlen, max_clients = MAX_CLIENTS;
    int client_sockets[MAX_CLIENTS] = {0};
    char buffer[BUFFER_SIZE];
    fd_set readfds;
    char *welcome_message = "Welcome to the chatroom!\n";

    // create master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // set master socket to allow multiple connections
    int opt = 1;
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        perror("Failed to set socket options");
        exit(EXIT_FAILURE);
    }

    // configure server address
    struct sockaddr_in address;
    addrlen = sizeof(address);
    memset(&address, 0, addrlen);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, addrlen) < 0)
    {
        perror("Failed to bind socket");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d...\n", PORT);

    // listen for incoming connections
    if (listen(master_socket, max_clients) < 0)
    {
        perror("Failed to listen for incoming connections");
        exit(EXIT_FAILURE);
    }

    // accept incoming connections
    while (1)
    {
        // clear the socket set
        FD_ZERO(&readfds);

        // add master socket to set
        FD_SET(master_socket, &readfds);

        // add child sockets to set
        int max_socket = master_socket;
        for (int i = 0; i < max_clients; i++)
        {
            int client_socket = client_sockets[i];
            if (client_socket > 0)
            {
                FD_SET(client_socket, &readfds);
            }
            if (client_socket > max_socket)
            {
                max_socket = client_socket;
            }
        }

        // wait for activity on one of the sockets
        activity = select(max_socket + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR))
        {
            perror("Select error");
        }

        // if activity is detected on master socket, a new connection has been made
        if (FD_ISSET(master_socket, &readfds))
        {
            if ((new_socket = accept(master_socket, (struct sockaddr *) & address, (socklen_t)&addrlen)) < 0)
            {
                perror("Failed to accept incoming connection");
                exit(EXIT_FAILURE);
            }

            // send welcome message to new client
            send(new_socket, welcome_message, strlen(welcome_message), 0);

            // add new socket to array of sockets
            for (int i = 0; i < max_clients; i++)
            {
                if (client_sockets[i] == 0)
                {
                    client_sockets[i] = new_socket;
                    printf("Client %d connected\n", i);
                    break;
                }
            }

            // check for IO operations on sockets
            for (int i = 0; i < max_clients; i++)
            {
                int socket = client_sockets[i];

                if (FD_ISSET(socket, &readfds))
                {
                    // check if it's a new connection or incoming data
                    if (socket == master_socket)
                    {
                        // handle new connection (already done above)
                    }
                    else
                    {
                        // handle incoming data
                        if (read(socket, buffer, BUFFER_SIZE) == 0)
                        {
                            // client disconnected
                            close(socket);
                            client_sockets[i] = 0;
                            printf("Client disconnected: socket fd = %d, client id = %d\n", socket, i);
                        }
                        else
                        {
                            // handle incoming message
                            if (client_sockets[i] == '\0')
                            {
                                // ask client for name
                                char prompt[] = "Please enter your name: ";
                                send(socket, prompt, strlen(prompt), 0);
                            }
                            else
                            {
                                // parse message and send to other clients
                                char message[BUFFER_SIZE];
                                memset(message, 0, BUFFER_SIZE);
                                sprintf(message, "%s: %s", client_sockets[i], buffer);
                                printf("%s\n", message);

                                for (int j = 0; j < max_clients; j++)
                                {
                                    if (j != i && client_sockets[j] > 0)
                                    {
                                        send(client_sockets[j], message, strlen(message), 0);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
        }
    }
    return 0;
}
