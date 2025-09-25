#ifndef MANAGER_H
#define MANAGER_H
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
#include "objects.h"
#include "internet.h"

#define STATUS_SIZE 6
#define MAX_PACKAGE_SIZE 33
#define MESSAGESHOLDER 4000
#define MAXFILENAME 2049

#define CONF_FILE NULL

extern requests_monitor req_monitor;
extern Clients_monitor clients_monitor;

extern pthread_mutex_t log_lock;
extern int logfd;
extern pthread_mutex_t sync_info_lock;
extern Vector sync_info_pairs;


void* grand_sync_request(void *buf_capacity);
void sync_threads_termination(requests_monitor  monitor);
int system_boot(int *sock ,int port , char *logfile);
int Read_config_file(char *conf_file);
void* handle_clients(void *argu);
char* get_timestamp();

#endif