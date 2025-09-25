#include<stdio.h>
#include<stdlib.h>
#include <fcntl.h>   
#include <unistd.h>  
#include <string.h>
#include <time.h>
#include "internet.h"
#include "vector.h"

// Returns current timestamp as a formatted string
char* get_timestamp() {
    static char buffer[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

// Updates the logfile with command usage
void uptade_logfile(int logfd , int args , char* op , char *source , char *target){
    char output[2024] = ""; 

    // Log ADD command with source and target
    if(args == 3){
        snprintf(output , sizeof(output) , "[%s] Command add %s -> %s\n" , get_timestamp() , source , target);
        if((write(logfd , output , strlen(output))) < strlen(output))
            perror("ERROR FAILED TO UPTADE LOGFILE:write");
    }
    // Log general commands with one argument
    else if (args == 2){
        snprintf(output , sizeof(output) , "[%s] Command %s %s\n" , get_timestamp() , op , source);
        if((write(logfd , output , strlen(output))) < strlen(output))
            perror("ERROR FAILED TO UPTADE LOGFILE:write");
    }
    // Log shutdown or commands without arguments
    else {
        snprintf(output , sizeof(output) , "[%s] Command %s\n" , get_timestamp() , op);
        if((write(logfd , output , strlen(output))) < strlen(output))
            perror("ERROR FAILED TO UPTADE LOGFILE:write");
    }
}

// Entry point: ./nfs_console -l <console-logfile> -h <host_IP> -p <host_port>
int main(int argv , char* argu[]){
    int log_fd , arguments , data = -1 , manager_fd , manager_port = atoi(argu[6]);
    char op[10] = "" , source_dir[1024] = "" , target_dir[1024] =""; 
    Vector Errors;

    if ((Errors = vector_create(1)) == NULL){
        return -1;
    }

    // Open or create log file
    if((log_fd = open(argu[2] , O_WRONLY | O_CREAT, 0644)) == -1){
        perror("FATAL ERROR :FAILED TO OPEN LOGFILE");
        return 1;
    }

    char datas[2024] = "";

    while(strcmp(op , "shutdown") != 0)
    {
        printf("> ");

        // Read command from user input
        if (fgets(datas, sizeof(datas), stdin) == NULL)
            break;

        datas[strcspn(datas, "\n")] = '\0'; // remove newline

        // Parse input into op, source, and target
        if((arguments = sscanf(datas, "%s %s %s", op, source_dir, target_dir)) == -1){
            perror("sscanf:Something went wrong with user's input\n");
        }
        // Log the command
        uptade_logfile(log_fd , arguments , op , source_dir , target_dir);

        // Attempt to connect to the NFS manager
        if((manager_fd = connect_to_server_by_name(manager_port , argu[4], Errors)) == -1){
            printf("Could not connect to %s\n" , argu[4]);
            continue;
        }

        // Send command to manager
        if((data = write(manager_fd , datas , strlen(datas)+1)) == -1){
            printf("Could not send command");
            close(manager_fd);
            continue;
        }

        // Read and display manager response.
        //The last manager worker will close the socket and so the console will know it got all the mesages
        //that was expecting.
        while(1){
            if((data = read(manager_fd ,datas , 2024))){
                printf("%s\n" , datas);
                if(write(log_fd , datas , strlen(datas)) == -1)
                    printf("%s" , strerror(errno));
            }

            if(data == 0){
                close(manager_fd);
                break;
            }
        }
    }
    Errors->destroy(Errors);
    close(log_fd);
    return 1;
}