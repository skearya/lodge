#include "shared.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>

void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int client_socket()
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	struct addrinfo *servinfo;
	int status = getaddrinfo(NULL, "4567", &hints, &servinfo);
	if (status != 0)
	{
		fprintf(stderr, "%s\n", gai_strerror(status));
		exit(1);
	}

	char str_addr[INET6_ADDRSTRLEN];
	inet_ntop(servinfo->ai_family, get_in_addr(servinfo->ai_addr), str_addr, sizeof str_addr);
	printf("connecting to %s\n", str_addr);

	int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if (sockfd == -1)
	{
		fatal_err("socket");
	}

	if (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
	{
		fatal_err("connect");
	}

	freeaddrinfo(servinfo);

	return sockfd;
}

int main(int argc, char *argv[])
{
	int clientfd = client_socket();

	struct pollfd fds[] =
		{
			{
				.events = POLLIN,
				.fd = STDIN_FILENO,
			},
			{
				.events = POLLIN,
				.fd = clientfd,
			},
		};

	message_t incoming_message;

	while (1)
	{
		int event_count = poll(fds, sizeof fds / sizeof(struct pollfd), -1);
		if (event_count == -1)
		{
			fatal_err("poll");
		}

		if (fds[0].revents & POLLIN)
		{
			char *input = NULL;
			size_t size = 0;
			ssize_t bytes_read = getline(&input, &size, stdin);
			if (bytes_read == -1)
			{
				fatal_err("stdin read");
			}

			ssize_t message_len = bytes_read - 1; /* remove one because of new line char */

			if (message_len != 0 && message_len <= MAX_MESSAGE_LEN)
			{
				message_t message;
				message.len = message_len;
				memcpy(message.buffer, input, message_len);

				ssize_t bytes_sent = send(clientfd, (char *)&message, message_len + 1 /* add one cause of first byte */, 0);
				if (bytes_sent == -1)
				{
					fatal_err("message send");
				}
			}
			else
			{
				if (message_len == 0)
				{
					printf("message cant be empty\n");
				}
				else
				{
					printf("message too long\n");
				}
			}

			free(input);
		}

		if (fds[1].revents & POLLIN)
		{
			ssize_t bytes_read = recv(clientfd, (char *)&incoming_message, sizeof incoming_message, 0);
			if (bytes_read <= 0)
			{
				fatal_err("message read");
			}
			incoming_message.buffer[incoming_message.len] = '\0';

			printf("%s\n", incoming_message.buffer);
		}
	}

	close(clientfd);

	return 0;
}