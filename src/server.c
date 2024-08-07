#define STB_DS_IMPLEMENTATION
#include "../lib/stb_ds.h"
#include "shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <errno.h>

#define MAX_EVENTS 64

typedef struct
{
    int bytes_recv;
    message_t message;
} client_t;

int server_socket()
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *servinfo;
    int status = getaddrinfo(NULL, PORT, &hints, &servinfo);
    if (status != 0)
    {
        fprintf(stderr, "%s\n", gai_strerror(status));
        exit(1);
    }

    int sockfd = -1;
    int yes = 1;

    for (struct addrinfo *info = servinfo; info != NULL; info->ai_next)
    {
        if ((sockfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1)
        {
            continue;
        }

        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

        if (bind(sockfd, info->ai_addr, info->ai_addrlen) == -1)
        {
            close(sockfd);
            sockfd = -1;
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (sockfd == -1)
    {
        fatal_err("getting socket");
    }

    if (listen(sockfd, 5) == -1)
    {
        fatal_err("listen");
    }

    return sockfd;
}

int main()
{
    int serverfd = server_socket();
    printf("listening on port " PORT "\n");

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        fatal_err("epoll create");
    }

    struct epoll_event accept_event = {
        .events = EPOLLIN,
        .data.fd = serverfd,
    };

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serverfd, &accept_event) == -1)
    {
        fatal_err("epoll add");
    }

    struct epoll_event events[MAX_EVENTS];
    struct
    {
        int key;
        client_t value;
    } *clients = NULL;

    while (1)
    {
        int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        for (int i = 0; i < event_count; i++)
        {
            if (events[i].data.fd == serverfd)
            {
                int clientfd = accept(serverfd, NULL, NULL);
                if (clientfd == -1)
                {
                    perror("client accept");
                    continue;
                }

                struct epoll_event client_event = {
                    .events = EPOLLIN,
                    .data.fd = clientfd,
                };

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clientfd, &client_event) == -1)
                {
                    perror("epoll add client");
                    continue;
                }

                client_t client = {
                    .bytes_recv = 0,
                };

                hmput(clients, clientfd, client);

                printf("accepted client, fd %d\n", clientfd);
            }
            else
            {
                client_t *client = &hmget(clients, events[i].data.fd);

                ssize_t bytes_read = recv(events[i].data.fd, (char *)&client->message + client->bytes_recv, MAX_MESSAGE_LEN - client->bytes_recv, 0);
                if (bytes_read <= 0)
                {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                    close(events[i].data.fd);
                    hmdel(clients, events[i].data.fd);
                    printf("removed fd %d due to disconnect/err\n", events[i].data.fd);
                    continue;
                }

                client->bytes_recv += bytes_read;

                if (client->message.len == client->bytes_recv - 1)
                {
                    for (int j = 0; j < hmlen(clients); j++)
                    {
                        if (clients[j].key != events[i].data.fd)
                        {
                            ssize_t bytes_sent = send(clients[j].key, &client->message, client->bytes_recv, 0);
                            if (bytes_sent == -1)
                            {
                                perror("message send");
                            }
                        }
                    }

                    printf("msg %s from fd %d | length %d\n", client->message.buffer, events[i].data.fd, client->message.len);

                    client->bytes_recv = 0;
                    client->message.len = 0;
                    memset(client->message.buffer, 0, sizeof(client->message.buffer));
                }
            }
        }
    }

    close(epoll_fd);
    close(serverfd);
    hmfree(clients);

    return 0;
}
