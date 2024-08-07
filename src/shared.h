#include <stdlib.h>
#include <stdio.h>

#define PORT "4567"
#define MAX_MESSAGE_LEN 128

typedef struct
{
    char len;
    char buffer[MAX_MESSAGE_LEN + 1];

} message_t;

void fatal_err(char *msg)
{
    perror(msg);
    exit(1);
}