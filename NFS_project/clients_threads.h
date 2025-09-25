// worker.h
#ifndef CLIENTSTHREADS_H
#define CLIENTSTHREADS_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>     // for errno
#include <dirent.h>
#include <fcntl.h>
#include "vector.h"
#include "internet.h"
#define MAX_PACHAGE 33
#define STATUS_SIZE 6
#define MAX_PACKAGE_SIZE 33
#define MESSAGESHOLDER 4000
#define MAXFILENAME 2049
#define MAX_DATA_CAPACITY 20000


void* handle_client(void* arg);

#endif