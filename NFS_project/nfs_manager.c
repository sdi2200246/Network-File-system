#include "nfsmanager_utils.h"

requests_monitor req_monitor;
Clients_monitor clients_monitor;
pthread_mutex_t log_lock;
int logfd;
pthread_mutex_t sync_info_lock;
Vector sync_info_pairs;


//./nfs_manager -l <manager_logfile> -c <config_file> -n <worker_limit> -p <port_number> -b <bufferSize>

int main(int argv , char *argu[]){
    Vector Errors = vector_create(1);
    int sock , port = atoi(argu[8]) , stream , BufferSize =  atoi(argu[10]) , rc;
    Client c_lient;
    char client_message[2048];
    pthread_t worker_threads_id[atoi(argu[6])] , client_threads; 
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);

    if(system_boot(&sock , port , argu[2]) == -1)
        return -1;

    //create thread_pool
    for(int i = 0 ; i < atoi(argu[6]) ; i++)
        if((rc = pthread_create(&worker_threads_id[i] , NULL , grand_sync_request ,(void*)&BufferSize))!= 0)
            printf("pthread_create failed: %s\n", strerror(rc)); // use rc directly

    //file must end just with a \n on the final pair line(..final pair\nEOF).
    Read_config_file(argu[4]);

    while(1){
        //in case something went wrong accept somone else.
        if((stream = accept(sock , (struct sockaddr *)&client , &client_len)) == -1){
            perror("accpet");
            continue;
        }
        if(read(stream , client_message , sizeof(client_message)) == -1){
            close(stream);
            continue;
        }   
        //shutdown command came.
        if(client_message[0] == 's'){
            close(stream);
            break;
        }
        c_lient = create_init_client(stream , client_message);

        if ((rc = pthread_create(&client_threads ,NULL , handle_clients , c_lient)) != 0)
            printf("%s" , strerror(rc));
       
        else {
            //add client in the system. 
            increase_clients(clients_monitor);
            pthread_detach(client_threads);
        }

    }
      // Graceful shutdown phase
    printf("[%s] Shutting down manager ...\n" , get_timestamp());
    printf("[%s] Waiting for all active workers to finish\n" , get_timestamp());
    printf("[%s] Processing remaining queued tasks\n" , get_timestamp());

    //wait for all your active clients to leave the system.
    destroy_Clients_monitor(clients_monitor);
    sync_threads_termination(req_monitor);
    for(int i = 0 ; i < atoi(argu[6]) ; i++)
        pthread_join(worker_threads_id[i] , NULL);

    //wait for the threads to finished the remainning tasks.
    destroy_requests_monitor(req_monitor);
    pthread_mutex_destroy(&log_lock);
    sync_info_pairs->destroy(sync_info_pairs);
    Errors->destroy(Errors);
    close(logfd);
    close(sock);
    printf("[%s] Manager shutdown complete\n" , get_timestamp());

    return 0;
   
}