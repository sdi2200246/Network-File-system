#ifndef REQ_H
#define REQ_H

#include<string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <errno.h>   
#include <fcntl.h>
#include <libgen.h>  // for dirname
#include "vector.h"


typedef struct Clients_monitor * Clients_monitor;
struct Clients_monitor{

    pthread_mutex_t lock;
    pthread_cond_t ready;
    int count;
};


typedef struct Pair_monitor * Pair_monitor;

struct Pair_monitor{

    pthread_mutex_t lock;
    pthread_cond_t  done , ready;
    int count , sock , files_no;
};

typedef struct Request * Request;

struct Request{
    int source_port , target_port ;
    char source_add[16] , target_add[16] , source_file[256] , target_file[256];
    Pair_monitor owner;
    void(*destroy)(struct Request*);
};

typedef struct Sync_info_pair * Sync_info_pair ;

struct Sync_info_pair{
    int source_port , target_port , status;
    char source_add[16] , target_add[16] , source_dir[256] , target_dir[256];;
    void(*destroy)(struct Sync_info_pair*);
};

typedef struct requests_monitor * requests_monitor;

struct requests_monitor{

    pthread_mutex_t lock;
    pthread_cond_t   empty;
    Vector sync_requests;
    int stop;
};

typedef struct Client * Client;

struct Client{
    int sock;
    char request[2048];
};

//struct Request.
Request create_init_req(int source_port , int target_port , Pair_monitor owner ,char* source_add, char* target_add , char * source_file , char *target_file);
void Destroy_request(Request req);
void print_request(Request req);

//Sync_info_pair functions.
Sync_info_pair create_init_syncpair(int source_port , int target_port ,char* source_add, char* target_add ,  char * s_source , char * t_source);
void Destroy_syncpair(Sync_info_pair pair);
int sync_info_pair_equals(Sync_info_pair pair ,int s_port  ,char* s_add, char * s_source);
Sync_info_pair find_pair(Vector syncs ,  char *s_dir , char* s_addr , int s_port);

//request_monitor.
requests_monitor requests_monitor_initialization();
Request get_request(requests_monitor  monitor);
void add_request(requests_monitor  monitor , Request  req);
void destroy_requests_monitor(requests_monitor  monitor);
void remove_requests(requests_monitor  monitor , char *s_dir , char *s_add , int s_port);

//Pair request.
Pair_monitor  pair_monitor_initialization(int sock ,int files_no);
void  increase_count(Pair_monitor  monitor);
void destroy_pair_monitor(Pair_monitor  monitor );
void ready_up(Pair_monitor monitor, int files);
void early_destroy_pair_monitor(Pair_monitor monitor , int removed);

//clinets obj.
Client create_init_client(int sock , char*);

Clients_monitor  Clients_monitor_initialization();
void  increase_clients(Clients_monitor  monitor);
void  decrease_clients(Clients_monitor  monitor);
void  destroy_Clients_monitor(Clients_monitor  monitor );

#endif