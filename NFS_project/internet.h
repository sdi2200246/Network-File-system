#ifndef INTERNET_H
#define INTERNET_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>  // For inet_pton()
#include <netdb.h>
#include <errno.h>     // for errno
#include "vector.h"

int socket_initilization_binding(int *Sock , int port);
int connect_to_server(int port , char *adress , Vector Errors);
int connect_to_server_by_name(int port , char *adress , Vector Errors);
#endif
