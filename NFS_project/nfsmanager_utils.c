#include "nfsmanager_utils.h"

// [Helper] Returns the current system timestamp as a formatted string (YYYY-MM-DD HH:MM:SS).
// Used for consistent logging timestamps across operations.
char* get_timestamp() {
    static char buffer[32];  // Static so it persists after function returns
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

// [Core] Performs full system initialization:
// - Initializes global data structures (e.g., sync_info_pairs)
// - Creates and binds a listening socket
// - Initializes request and client monitors
// - Sets up mutexes for thread-safe logging and sync info access
// - Opens the log file for writing
int system_boot(int *sock  , int port , char *log_file){

    if((sync_info_pairs = vector_create(1)) == NULL){
        perror("Failed to initialize vital datastructure:malloc");
        return -1;
    }
    //create the socket that  the manager is going to listen to.
    if(socket_initilization_binding(sock , port) == -1){
        printf("System boot failed:socket initialization");
        sync_info_pairs->destroy(sync_info_pairs);
        return -1;
    }
    //this is the monitor for the queye that thr requests live.(PULL PUSH).
    req_monitor = requests_monitor_initialization();
     if(req_monitor == NULL){
        close(*sock);
        sync_info_pairs->destroy(sync_info_pairs);
        return -1;
     }
    // Initialize mutex with default attributes
    if (pthread_mutex_init(&log_lock, NULL) != 0) {
        close(*sock);
        sync_info_pairs->destroy(sync_info_pairs);
        destroy_requests_monitor(req_monitor);
        perror("System boot failed:pthread_mutex_init");
        return -1;
    }
    if(pthread_mutex_init(&sync_info_lock, NULL) != 0){
        close(*sock);
        sync_info_pairs->destroy(sync_info_pairs);
        destroy_requests_monitor(req_monitor);
        pthread_mutex_destroy(&log_lock);
        perror("System boot failed:pthread_mutex_init");
        return -1;

    }
    if((clients_monitor = Clients_monitor_initialization()) == NULL){
        close(*sock);
        sync_info_pairs->destroy(sync_info_pairs);
        destroy_requests_monitor(req_monitor);
        pthread_mutex_destroy(&log_lock);
        perror("System boot failed:pthread_mutex_init");
        return -1;
    }


    //opening the logfile.
    if((logfd = open(log_file , O_WRONLY|O_CREAT | O_TRUNC)) == -1){
        perror("System boot failed:open");
        close(*sock);
        sync_info_pairs->destroy(sync_info_pairs);
        destroy_requests_monitor(req_monitor);
        pthread_mutex_destroy(&log_lock);
        pthread_mutex_destroy(&sync_info_lock);
        pthread_mutex_destroy(&clients_monitor->lock);
        pthread_cond_destroy(&clients_monitor->ready);
        free(clients_monitor);
        return -1;
    }
    return 1;
}

// [Helper] Reads a protocol header from the socket stream.
// Extracts the status string ("OK" or "ERROR") and the accompanying data size.
int read_header(int sock, char *status, char *data_size, Vector Error) {
    int count = 0, sindex = 0, pindex = 0;
    char byte;
    ssize_t n;

    while (count < 2) {
        if ((n = read(sock, &byte, 1)) == -1) {
            Error->push_back(Error, strdup(strerror(errno)));
            return -1;
        } 
        else if (n == 0) {
            // EOF reached unexpectedly
            Error->push_back(Error, strdup("Unexpected EOF"));
            return -1;
        }
        if (byte == ' ') {
            count++;
            continue;
        }

        if(sindex >= STATUS_SIZE || pindex >= MAX_PACKAGE_SIZE){
            Error->push_back(Error, strdup("Unexpected Erros"));
            return -1;
        }

        if (count == 0)
            status[sindex++] = byte;
        if (count == 1)
            data_size[pindex++] = byte; 
    }

    status[sindex] = '\0';
    data_size[pindex] = '\0';

   

    return 1;
}
// [Helper] Reads an error message of specified size from the socket and
// stores it in the Errors vector for future reporting or logging.
int read_Error(int sock , Vector Errors , int cap , int size){

    char message[cap];
    if((read(sock , message , size)) == -1){
        Errors->push_back(Errors , strdup("FAILED TO RECEIVE ERROR STATUS."));
        return -1;
    }
    message[size] ='\0';
    Errors->push_back(Errors , strdup(message));
    return 1;
}
// [Core] Constructs and sends a complete PUSH request to a remote peer:
// - Includes command, filename, and optional data payload (chunk)
// - Expects a response header and error message if applicable
int push_request(int sock , int cap , int chunk , char *data , char *file ,Vector Errors){
    char status[MESSAGESHOLDER] ="" , Pushreq[2*cap] , datasize[MAX_PACKAGE_SIZE+1] ="";
    int offset = 0;
    //Add command
    memcpy(Pushreq + offset, "PUSH ", 5);
    offset += 5;
    //Add filename
    int len = strlen(file);
    memcpy(Pushreq + offset, file, len);
    offset += len;

    //Add space
    Pushreq[offset++] = ' ';
    // Add chunk size
    char chunk_str[33];
    int chunk_len = snprintf(chunk_str, sizeof(chunk_str), "%d", chunk);
    memcpy(Pushreq + offset, chunk_str, chunk_len);
    offset += chunk_len;
    Pushreq[offset++] = ' ';

    //Add space
    if (data) {
        // Add data
        memcpy(Pushreq + offset, data, chunk);
        offset += chunk;
    }
    Pushreq[offset] = '\0';

    if((write(sock , Pushreq , strlen(Pushreq))) == -1){
        Errors->push_back(Errors , strdup(strerror(errno)));
        return -1;
    }
    read_header(sock , status , datasize , Errors);
    if(!Errors->size && !strcmp(status , "ERROR")){
        read_Error(sock , Errors , cap , atoi(datasize));
        return -1;
    }
    return 1;
}

// [Core] Transfers a file to a target server using multiple push_request calls.
// - First sends a command to initiate upload
// - Splits file into chunks and sends each chunk
// - Sends final signal to indicate end of file
int push_data(int source_sock , int target_sock , int data_size , int cap, Vector Errors , char *target_file){
    char buffer[cap+1] ;
    int  bytes_left , bytes_sent , bytes_to_read;

    //inform clinet about to send chunks of data.
    push_request(target_sock , cap , -1 , NULL , target_file , Errors);

    if(!Errors->size){
        bytes_left = data_size;

        //send chunks of data.
        while(bytes_left > 0 && !Errors->size){
            //cap is the buffer's size .check how many bytes to read.
            bytes_to_read = (bytes_left > cap) ? cap : bytes_left;
            //read chunk of data
            if((bytes_sent = read(source_sock , buffer , bytes_to_read)) <= 0){
                Errors->push_back(Errors , strdup(strerror(errno)));
                return -1;
            }
            //sned package
            push_request(target_sock , cap , bytes_sent , buffer , target_file , Errors);
            bytes_left -= bytes_sent;
        }
        //infrom clinet that we have sent all the data chunks.
        if(!Errors->size)
            push_request(target_sock , cap , 0 , NULL , target_file , Errors);
    }
    return 1;
}
// [Logging] Writes a detailed operation result to both log file and, optionally, to the requesting client.
// Includes timestamp, source/target file info, thread ID, operation status, and error details.
void update_log_file(int bytes , Request req , Vector Errors , char *op ,char *op2){

    char log_message[MESSAGESHOLDER] = "" , client_message[MESSAGESHOLDER] = "" , *error;
    //failure case
    if(Errors->size){
        error = (char*)Errors->pop(Errors);
        snprintf(log_message , sizeof(log_message) , "[%s][%s@%s:%d][%s@%s:%d][%lu][%s][ERROR][File:%s-%s]\n" , get_timestamp() , req->source_file,req->source_add , req->source_port , req->target_file , req->target_add , req->target_port, pthread_self() , op , req->source_file ,error); 
        snprintf(client_message , sizeof(client_message) , "[%s][ERROR][%s] file :%s@%s:%d -> %s@%s:%d\n" , get_timestamp() , error , req->source_file,req->source_add , req->source_port , req->target_file , req->target_add , req->target_port); 
        free(error);
        
    }
    else {
        snprintf(log_message , sizeof(log_message) , "[%s][%s@%s:%d][%s@%s:%d][%lu][%s][SUCCES][File:%s-%d bytes %s]\n" , get_timestamp() ,req->source_file,req->source_add , req->source_port , req->target_file , req->target_add , req->target_port, pthread_self() , op ,req->source_file , bytes , op2);
        snprintf(client_message , sizeof(client_message) , "[%s]Added file :%s@%s:%d -> %s@%s:%d\n" , get_timestamp() , req->source_file,req->source_add , req->source_port , req->target_file , req->target_add , req->target_port); 
    }
  
    //Critical Section loogind section.
    pthread_mutex_lock(&log_lock);

    if(write(logfd , log_message , strlen(log_message)) == -1)
        printf("%s" , strerror(errno));

    if(req->owner && (!strcmp(op , "PUSHED") || Errors->size)){
        printf("%s" , client_message);

        if(write(req->owner->sock , client_message , strlen(client_message)+1) == -1)
            printf("%s" , strerror(errno));

        if(write(logfd , client_message , strlen(client_message)) == -1)
            printf("%s" , strerror(errno));
    }
    pthread_mutex_unlock(&log_lock);

}
// [Handler] Worker thread function that continuously pulls sync requests from the global queue.
// - Performs PULL from source server
// - Then performs PUSH to target server
// - Logs all results and cleans up sockets and request state
void* grand_sync_request(void *buf_capacity){

    int cap = *(int*)buf_capacity;
    Vector Errors = vector_create(1);
    char status[STATUS_SIZE] = "" , datasize[MAX_PACKAGE_SIZE] ="" , message[cap];
    int  source_sock , target_sock;
    Request req;

    while(1){
      
        req = get_request(req_monitor);

        if(req == NULL)
            break;
        //connect to servers.
        source_sock = connect_to_server(req->source_port , req->source_add , Errors);
        target_sock = connect_to_server(req->target_port , req->target_add , Errors);
        
        //send the PULL request first.
        if(!Errors->size){
            snprintf(message , sizeof(message) , "PULL %s" , req->source_file);
            if(write(source_sock , message , strlen(message)) == -1){
                Errors->push_back(Errors , strdup(strerror(errno)));
                update_log_file(atoi(datasize) , req , Errors , "PULLED" , "pulled");
            }
            else {
                read_header(source_sock , status , datasize , Errors);
                if(!Errors->size && !strcmp(status , "ERROR"))
                    read_Error(source_sock , Errors ,cap , atoi(datasize));
                update_log_file(atoi(datasize) , req , Errors , "PULLED" , "pulled");
            }
        }
        //send the PUSH packages.
        if(!Errors->size && !strcmp(status , "OK")){
            push_data(source_sock , target_sock , atoi(datasize) , cap , Errors , req->target_file);
            update_log_file(atoi(datasize) , req , Errors , "PUSHED" , "pushed");
        }
        if(req->owner)
            increase_count(req->owner);
        req->destroy(req);
        close(target_sock);
        close(source_sock);
    }
    Errors->destroy(Errors);
    pthread_exit(NULL);
}

// [Helper] Notifies all worker threads to stop gracefully by broadcasting to condition variable
// and setting a stop flag in the shared monitor.
void sync_threads_termination(requests_monitor  monitor){

    pthread_mutex_lock(&monitor->lock);
    monitor->stop = 1;
    pthread_cond_broadcast(&monitor->empty);
    pthread_mutex_unlock(&monitor->lock);  
}

// [Helper] Parses a config file line containing two directory/server paths.
// Extracts source and target paths, IP addresses, and ports.
void parse_config_line(const char *line, 
                      char *source_dir, char *source_host, int *source_port,
                      char *target_dir, char *target_host, int *target_port) {
    // Example line: "/source1@123.10.10.20:8000 /source2@100.200.10.10:8080"
    sscanf(line, "/%[^@]@%[^:]:%d /%[^@]@%[^:]:%d",
           source_dir, source_host, source_port,
           target_dir, target_host, target_port);
}

//[Core] Sends a LIST request to the specified source server and receives a list of filenames.
// Populates the provided vector with names of available files in the directory.
int Read_List(int sock ,int cap , char * source , Vector files , Vector Errors){
    char Listreq[2*cap] , byte[1] =" " , chunkzise[MAX_PACKAGE_SIZE] , status[STATUS_SIZE] , file[MAXFILENAME];
    // create request.
    snprintf(Listreq , sizeof(Listreq) , "LIST %s" , source);
    //send request.
    if(write(sock , Listreq , strlen(Listreq)) == -1){
        Errors->push_back(Errors , strdup(strerror(errno)));
        return -1;
    }
    //read header answer.
    if((read_header(sock , status , chunkzise , Errors) == -1)){
        return -1;
    }

    //something went wrong on the clinets side.
    if(!strcmp(status , "ERROR")){
        read_Error(sock  , Errors , cap , atoi(chunkzise));
        return -1;
    }

    //read file names and push them in the files Vector.
    int index=0; 
    while(1){
        if(read(sock , byte , 1) == -1){
            Errors->push_back(Errors , strdup(strerror(errno)));
            return -1;
        }
        if(*byte == '.' && index == 0)
            break;

        if(*byte == '\n'){
            file[index] ='\0';
            files->push_back(files , strdup(file));
            index = 0;
            continue;
        }
        file[index++] = *byte;
    }

    return 1;
}
// [Core] Adds all sync requests to the global queue for files from a source directory.
// - Connects to source server
// - Gets file list
// - Constructs Request objects for each file
// - Pushes requests into global request queue
int add_requests(char *source , char*target , char *source_add , char *target_add , int source_port ,int target_port ,Pair_monitor owner , Vector Errors){

    Vector files;
    int source_sock , files_sent;
    Request req;
    Sync_info_pair newpair;
    char *file  , source_file[MAXFILENAME] , target_file[MAXFILENAME];
    if((files = vector_create(1)) == NULL)
        return -1;

    //connect to the source_dir adress.
    if((source_sock = connect_to_server(source_port , source_add , Errors)) == -1){
        return -1;
    }

    //Read the names of the files and save them in the files vector.
    if(Read_List(source_sock , MESSAGESHOLDER*2 , source , files , Errors) == -1){
        return -1;
    }
    //owner is a monitor resembling a the following task.For every pair 
    //if the worker is done with the tranfer of the file it adds it to the completed jobs
    //until all the files of the pair are moved.
    files_sent = files->size;
    if(owner)
        owner->files_no = files_sent;

    
    //create requests and push them in the queue for the workers.
    while(files->size){
        file = (char*)files->pop(files);
        snprintf(source_file , sizeof(source_file) , "%s/%s" , source , file);
        snprintf(target_file , sizeof(target_file) , "%s/%s" , target , file);
        req = create_init_req(source_port , target_port ,owner, source_add , target_add , source_file , target_file);
        //end of critical section.
        pthread_mutex_lock(&sync_info_lock);
        if(req)
            add_request(req_monitor , req);
        
        newpair = create_init_syncpair(source_port , target_port , source_add , target_add , source , target);
        sync_info_pairs->push_back(sync_info_pairs , newpair);
        //end of critical section.
        pthread_mutex_unlock(&sync_info_lock);
        free(file);
    }
    close(source_sock);
    files->destroy(files);
    return files_sent;
}
// [Core] Reads the synchronization configuration file line by line.
// For each entry, parses the config and invokes add_requests to schedule file transfers.
int Read_config_file(char *conf_file){

    int cfd , lineindex = 0 ,n;
    char source_dir[MAXFILENAME], source_host[MAXFILENAME] , target_dir[MAXFILENAME], target_host[MAXFILENAME] , line[3*MAXFILENAME] , byte;
    int source_port, target_port;
    Vector Errors = vector_create(1);


    if(Errors == NULL){
        perror("[FATAL_ERROR][FAILED TO OPEN CONFIGURATION FILE.]");
        Errors->destroy(Errors);
        return -1;
    }

    if((cfd = open(conf_file , O_RDONLY)) == -1){
        perror("[FATAL_ERROR][FAILED TO OPEN CONFIGURATION FILE.]");
        Errors->destroy(Errors);
        return -1;
    }

    //Reads one bytes form each line of the file.
    while((n = read(cfd , &byte , 1)) != 0){
        if(byte == '\n'){
            line[lineindex] = '\0';
            parse_config_line(line , source_dir , source_host , &source_port , target_dir , target_host , &target_port);

            if(add_requests(source_dir , target_dir , source_host , target_host , source_port , target_port , CONF_FILE , Errors) == -1)
                printf("Configuration file error: %s@%s:%d - %s\n" , source_dir , source_host , source_port, (char*)Errors->at(Errors , 0));
                
            lineindex = 0;
            continue;
            }
        line[lineindex++] = byte;
    }
    if(n == -1){
        perror("[FATAL_ERROR][FAILED TO READ CONFIGURATION FILE.]");
        return -1;
    }
    close(cfd);
    Errors->destroy(Errors);
    return 1;
}

// [Helper] Parses a raw client request string (e.g., "add /dir@host:port")
// Splits it into the command keyword and its data payload.
void split_clients_req(const char *clients_request , char *req , char *data){

    int index_r = 0 , index_d = 0 , index_cr=0 , spaces = 0;
    while(clients_request[index_cr]){
        if(clients_request[index_cr] == ' ')
            spaces++;

        if(spaces == 1 && clients_request[index_cr] == ' '){
            index_cr += 1;
            continue;    
        }

        if(spaces == 0 )
            req[index_r++] = clients_request[index_cr++];

        else data[index_d++] = clients_request[index_cr++];
    }
    req[index_r] = '\0';
    data[index_d] = '\0';

}
// [Handler] Processes a client "add" command to start syncing a new directory.
// - Checks for existing sync activity
// - Schedules sync jobs using add_requests
// - Responds to the client with appropriate status
void handle_add_request(char *data , int clients_sock , Vector Errors){
    char source_dir[MAXFILENAME], source_host[MAXFILENAME] , target_dir[MAXFILENAME], target_host[MAXFILENAME] , clinets_answer[3*MAXFILENAME];
    int source_port , target_port;
    Pair_monitor monitor;
    Sync_info_pair pair , newpair;

    parse_config_line(data , source_dir , source_host , &source_port , target_dir  , target_host , &target_port);

    //entering critical section.
    pthread_mutex_lock(&sync_info_lock);
    pair = find_pair(sync_info_pairs  , source_dir  , source_host , source_port);

    //case where there is a syncronazation in ptogress.
    if(pair && pair->status == 1){
        snprintf(clinets_answer , sizeof(clinets_answer) , "[%s] Already in queue: /%s@%s:%d\n" , get_timestamp() , source_dir , source_host , source_port);
        printf("%s" , clinets_answer);
        if(write(clients_sock , clinets_answer , strlen(clinets_answer)+1) == -1)
            printf("ANswer to client's request failed:%s" , strerror(errno));
        
        pthread_mutex_unlock(&sync_info_lock);
        return;
    }

    //case where there is no sync in progress.
    if(pair == NULL){
        newpair = create_init_syncpair(source_port , target_port , source_host , target_host , source_dir , target_dir);
        sync_info_pairs->push_back(sync_info_pairs , newpair);
        pair = newpair;
    }
    pair->status = 1;
    //exiting critical section.
    pthread_mutex_unlock(&sync_info_lock);
    
    if((monitor = pair_monitor_initialization(clients_sock , 0)) == NULL){
        pthread_mutex_lock(&sync_info_lock);
        pair->status = 0;
        pthread_mutex_unlock(&sync_info_lock);
    }
    
    add_requests(source_dir , target_dir , source_host , target_host , source_port , target_port , monitor , Errors);

    // ready_up(monitor , files_sent);
    destroy_pair_monitor(monitor);
    pthread_mutex_lock(&sync_info_lock);
    pair->status = 0;
    pthread_mutex_unlock(&sync_info_lock);
    return;


}
// [Handler] Processes a client "cancel" command to stop a sync job.
// - Checks if sync is active
// - Removes queued requests and notifies the client
void handle_cancel_request(char *data , int clients_sock , Vector Errors){

    char source_dir[MAXFILENAME], source_host[MAXFILENAME] , clinets_answer[3*MESSAGESHOLDER];
    int source_port ;
    Sync_info_pair pair;

    parse_config_line(data , source_dir , source_host , &source_port , NULL , NULL , NULL);
    
    //entering critical section.
    pthread_mutex_lock(&sync_info_lock);
    pair = find_pair(sync_info_pairs  , source_dir , source_host  , source_port);

    //[2025-02-10 10:00:01] Directory not being synchronized:/dir1/file1@1.2.3.4:8080
    if(pair == NULL || pair->status == 0){
        snprintf(clinets_answer , sizeof(clinets_answer) , "[%s] Directory not being synchronized :/%s@%s:%d\n" , get_timestamp() , source_dir , source_host , source_port);
        printf("%s" , clinets_answer);
        pthread_mutex_lock(&log_lock);
         if(write(clients_sock , clinets_answer , strlen(clinets_answer)+1) == -1)
            printf("ANswer to client's request failed:%s" , strerror(errno));
        
        pthread_mutex_unlock(&log_lock);
    }
    else {
        remove_requests(req_monitor , source_dir , source_host , source_port);
        snprintf(clinets_answer , sizeof(clinets_answer) , "[%s]Synchronization stopped for:/%s@%s:%d\n" , get_timestamp() , source_dir , source_host , source_port);
        printf("%s" , clinets_answer);
        pthread_mutex_lock(&log_lock);
         if(write(clients_sock , clinets_answer , strlen(clinets_answer)+1) == -1)
            printf("ANswer to client's request failed:%s" , strerror(errno));
        pthread_mutex_unlock(&log_lock);
    }

    pthread_mutex_unlock(&sync_info_lock);
    return;
}
// [Handler] Entry point for threads that handle high-level client commands.
// - Supports "add" and "cancel" requests
// - Splits input, delegates to proper handler, manages connection lifecycle
void* handle_clients(void *argu){

    char req[10] , data[MESSAGESHOLDER];
    Vector Errors = vector_create(1);

    Client client = (Client)argu;

    if(Errors == NULL || client == NULL){
        close(client->sock);
        printf("handle_clinets error:%s" , strerror(errno));
        free(argu);
        pthread_exit(NULL);
    }

    split_clients_req(client->request , req , data);

    if(!strcmp(req , "add"))
        handle_add_request(data , client->sock , Errors);


    if(!strcmp(req , "cancel"))
        handle_cancel_request(data , client->sock , Errors);
    
    close(client->sock);
    free(client);
    Errors->destroy(Errors);
    decrease_clients(clients_monitor);
    pthread_exit(NULL);
}