#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include "clients_threads.h"


//the socket is return via the pointer in *sock.

int main(int arg , char *argv[]){
   
    int mysock , port_no , stream , rc , *stream2 , count = 0;
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    pthread_t threats_id;

    //arguments check.
    if(arg < 3){
        printf("WRONG ARGUMENTS GIVEN THE PROGRAMM IS EXITING");
        return 0;
    }
    port_no = atoi(argv[2]);

    //socket creation - binding - listening.
    if((socket_initilization_binding(&mysock , port_no)) == -1){
        printf("FATAL ERROR , PROGRAMM IS EXITTING");
        return 0;
    }
    while(1){
        //in case something went wrong accept somone else.
        if((stream = accept(mysock , (struct sockaddr *)&client , &client_len)) == -1){
            perror("accpet");
            continue;
        }
        //avoid race conditions for the socket of each thread with malloc.
        stream2 = malloc(sizeof(int));
        *stream2 = stream;
        if ((rc = pthread_create(&threats_id ,NULL , handle_client , stream2)) != 0)
            printf("%s" , strerror(rc));
       
        count+=1;
        printf("Got client no: %d\n" , count);
        pthread_detach(threats_id);
    }

    close(stream);
}