#include"internet.h"

int socket_initilization_binding(int *sock , int port){
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    if((*sock = socket(AF_INET , SOCK_STREAM , 0)) == -1){
        perror("socket initiliazation failed");
        return -1;
    }  
    if(bind(*sock , (struct sockaddr*)&server , sizeof(server)) == -1){
        perror("socket binding failed");
        return -1;

    }
    if((listen(*sock , 10)) == -1){
        perror("socket listening failed");
        return -1;
    }
    return 0;
}

int connect_to_server(int port , char *adress , Vector Errors){

    int sockfd ; 
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        Errors->push_back(Errors , strdup(strerror(errno)));
        return -1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);  

    // Convert IP string to network byte order
    if (inet_pton(AF_INET, adress, &server.sin_addr) <= 0) {
        close(sockfd);
        Errors->push_back(Errors , strdup(strerror(errno)));
        return -1;
    }

    // Connect to server
    if (connect(sockfd, (struct sockaddr*)&server, sizeof(server)) < 0) {
        close(sockfd);
        Errors->push_back(Errors , strdup(strerror(errno)));
        return -1;
    }
    return sockfd;
}

int connect_to_server_by_name(int port , char* name , Vector Errors){
    struct hostent * mymachine ;
    struct in_addr ** addr_list ;

    if ( ( mymachine = gethostbyname (name) ) == NULL ){
        printf ( " Could not resolved Name : %s \n " , name);
        return -1;
    }
    else {
        addr_list = ( struct in_addr **) mymachine -> h_addr_list ;
        return connect_to_server(port , inet_ntoa(*addr_list[0]) , Errors);
    }

}