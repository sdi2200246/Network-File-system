#include "objects.h"

// [Factory] Allocates and initializes a new Request object with source and target info.
// Sets its destroy method and associates it with a pair monitor.
Request create_init_req(int source_port , int target_port , Pair_monitor owner , char* source_add , char* target_add , char * source_file , char *target_file){

    Request temp = malloc(sizeof(struct Request));
    if(temp == NULL){
        printf("FAILED TO CREATE REQUEST \n");
        return NULL;
    }

    strcpy(temp->source_add ,source_add);
    strcpy(temp->target_add, target_add);
    strcpy(temp->source_file , source_file);
    strcpy(temp->target_file , target_file);    
    temp->source_port = source_port;
    temp->target_port = target_port;
    temp->owner = owner;
    temp->destroy = Destroy_request;

    return temp;
}
// [Destructor] Frees memory associated with a Request object.
void Destroy_request(Request req){
    free(req);
}
// [Utility] Prints key details of a Request object to stdout for debugging.
void print_request(Request req){

    printf("SOURCE FILE:%s\nTARGETFILE:%s\nSOURCE ADD:%s\nTARGETADD:%s\n" , req->source_file , req->target_file , req->source_add , req->target_add);
    return;
}
// [Comparator] Checks if a request matches a given source directory, address, and port.
// Used for matching/canceling requests.
int equal_request(Request req , char *s_dir , char *s_add , int s_port){
    char req_source[1024];
    strcpy(req_source, req->source_file);  // dirname() may modify the string
    char *dir = dirname(req_source);
    return (strcmp(dir , s_dir) == 0 && strcmp(req->source_add , s_add) == 0 && req->source_port == s_port);
}
// [Factory] Allocates and initializes a Sync_info_pair struct to track a sync session.
// Stores addresses, ports, directories, and default status.
Sync_info_pair create_init_syncpair(int source_port , int target_port ,  char* source_add , char* target_add ,char * s_source , char * t_source){

    Sync_info_pair temp = malloc(sizeof(struct Sync_info_pair));
    if(temp == NULL){
        printf("FAILED TO CREATE REQUEST \n");
        return NULL;
    }

    strcpy(temp->source_add , source_add);
    strcpy(temp->target_add , target_add);
    strcpy(temp->source_dir , s_source);
    strcpy(temp->target_dir , t_source);   
    temp->source_port = source_port;
    temp->target_port = target_port;
    temp->status = 0;
    temp->destroy = Destroy_syncpair;

    return temp;
}
// [Comparator] Compares a sync pair against a directory, address, and port.
// Returns 1 if it matches the given sync source.
int sync_info_pair_equals(Sync_info_pair pair ,int s_port ,char* s_add , char * s_source){
    return !(pair->source_port != s_port || strcmp(s_source , pair->source_dir) != 0 || strcmp(s_source , pair->source_dir) != 0);

}
// [Search] Finds a matching Sync_info_pair from a vector of pairs.
// Returns the matching pair or NULL.
Sync_info_pair find_pair(Vector syncs ,  char *s_dir , char* s_addr , int s_port){

    Sync_info_pair temp = NULL;

    for(int i = 0 ; i < syncs->size ; i++){
        temp = (Sync_info_pair)syncs->at(syncs , i);
        if(sync_info_pair_equals(temp , s_port, s_addr , s_dir) == 1)
            return temp;
    }
    return temp;
}

// [Destructor] Frees memory allocated for a Sync_info_pair.
void Destroy_syncpair(Sync_info_pair pair){
    free(pair);
}

// [Factory] Initializes and returns a new requests_monitor struct.
// Sets up mutex, condition variable, and request queue.
requests_monitor requests_monitor_initialization(){
   
    requests_monitor  monitor;
    if((monitor = malloc(sizeof(struct requests_monitor))) == NULL){
        perror("malloc");
        return NULL;
    }

        // Initialize mutex with default attributes
    if (pthread_mutex_init(&monitor->lock, NULL) != 0) {
        perror("pthread_mutex_init");
        free(monitor);
        return NULL;
    }

    // Initialize condition variable with default attributes
    if (pthread_cond_init(&monitor->empty, NULL) != 0) {
        perror("pthread_cond_init");
        pthread_mutex_destroy(&monitor->lock);
        free(monitor);
        NULL;
    }
    
    if((monitor->sync_requests = vector_create(1)) == NULL){
        pthread_mutex_destroy(&monitor->lock);
        pthread_cond_destroy(&monitor->empty);
        free(monitor);
        return NULL;
    }
    monitor->stop = 0;

    return monitor;
}
// [Consumer] Waits for and retrieves a request from the monitor's queue.
// Blocks if no request is available and stop flag is not set.
Request get_request(requests_monitor  monitor){

    Request req = NULL;
    pthread_mutex_lock(&monitor->lock);

    while(monitor->sync_requests->size == 0 && monitor->stop == 0)
        pthread_cond_wait(&monitor->empty , &monitor->lock);  

    if(monitor->sync_requests->size)
        req =  (Request)monitor->sync_requests->pop(monitor->sync_requests);
        
    pthread_mutex_unlock(&monitor->lock);

    return req;
}
// [Producer] Adds a new Request to the monitor's request queue.
// Signals waiting threads that a new item is available.
void add_request(requests_monitor  monitor , Request  req){

    pthread_mutex_lock(&monitor->lock);
    monitor->sync_requests->push_back(monitor->sync_requests , req);
    pthread_cond_broadcast(&monitor->empty);
    pthread_mutex_unlock(&monitor->lock);
    
}
// [Modifier] Removes all requests from the queue that match a specific sync source.
// Also notifies the pair monitor of removed requests.
void remove_requests(requests_monitor  monitor , char *s_dir , char *s_add , int s_port){
    Pair_monitor p_monitor = NULL;
    Vector syncs_queue;
    Request temp;
    int flag =1 , index = -1 , removed = 0;

    pthread_mutex_lock(&monitor->lock);
    syncs_queue = monitor->sync_requests;

    while(flag){
        flag = 0;
        for(int i = 0 ; i < syncs_queue->size ; i++){
            temp = (Request)syncs_queue->at(syncs_queue , i);
                if(equal_request(temp , s_dir , s_add , s_port) == 1){
                    flag = 1;
                    p_monitor = temp->owner;
                    index = i; 
                    break;
                }
            }
        if(flag){
            temp = syncs_queue->at(syncs_queue , index);
            temp->destroy(temp);
            syncs_queue->delete_at(syncs_queue , index);
            removed +=1;
        }
    }
    if(p_monitor){
        early_destroy_pair_monitor(p_monitor , removed);
    }
    pthread_mutex_unlock(&monitor->lock);
}

// [Destructor] Destroys a requests_monitor, including mutexes, condition vars, and request queue.
void destroy_requests_monitor(requests_monitor  monitor){

    pthread_mutex_destroy(&monitor->lock);
    pthread_cond_destroy(&monitor->empty);
    monitor->sync_requests->destroy(monitor->sync_requests);
    free(monitor);
}
// [Factory] Initializes a Pair_monitor to track completion of a sync job.
// Associates it with a socket and file count, and sets up mutexes and condition variables.
Pair_monitor  pair_monitor_initialization(int socket ,int files_no){
    Pair_monitor  monitor;
    if((monitor = malloc(sizeof(struct Pair_monitor))) == NULL){
        perror("malloc");
        return NULL;
    }

        // Initialize mutex with default attributes
    if (pthread_mutex_init(&monitor->lock, NULL) != 0) {
        perror("pthread_mutex_init");
        free(monitor);
        return NULL;
    }

    // Initialize condition variable with default attributes
    if (pthread_cond_init(&monitor->done, NULL) != 0) {
        perror("pthread_cond_init");
        pthread_mutex_destroy(&monitor->lock);
        free(monitor);
        return NULL;
    }
    // Initialize condition variable with default attributes
    if (pthread_cond_init(&monitor->ready, NULL) != 0) {
        perror("pthread_cond_init");
        pthread_mutex_destroy(&monitor->lock);
        pthread_cond_destroy(&monitor->done);
        free(monitor);
        return NULL;
    }
    
    monitor->count = 0;
    monitor->sock = socket;
    monitor->files_no = files_no;

    return monitor;

}
// [Tracker] Increments the completed file count for a Pair_monitor.
// Signals completion condition if all files are done.
void increase_count(Pair_monitor  monitor){
    pthread_mutex_lock(&monitor->lock);

    monitor->count+=1;
    if(monitor->count == monitor->files_no)
        pthread_cond_signal(&monitor->done);
    
    pthread_mutex_unlock(&monitor->lock);
}
// [Modifier] Reduces the expected file count for a monitor (e.g., when canceling syncs early).
void early_destroy_pair_monitor(Pair_monitor monitor , int removed){

    pthread_mutex_lock(&monitor->lock);
    monitor->files_no -= removed;
    pthread_mutex_unlock(&monitor->lock);
}
// [Destructor] Blocks until all files are processed, then destroys the Pair_monitor.
void destroy_pair_monitor(Pair_monitor  monitor ){

    pthread_mutex_lock(&monitor->lock);

    while(monitor->count < monitor->files_no)
        pthread_cond_wait(&monitor->done , &monitor->lock);

    pthread_mutex_unlock(&monitor->lock);
    
    pthread_mutex_destroy(&monitor->lock);
    pthread_cond_destroy(&monitor->done);
    pthread_cond_destroy(&monitor->ready);

    free(monitor);    
}
// [Factory] Allocates and initializes a new Client object with socket and request data.
Client create_init_client(int sock , char * req){

    Client temp = malloc(sizeof(struct Client));
    if(temp == NULL){
        printf("Failed to create client:%s" , strerror(errno));
        return NULL;
    }
    temp->sock = sock;
    strcpy(temp->request , req);
    
    return temp;
}
// [Factory] Initializes a Clients_monitor struct to track connected clients.
// Sets up mutex and condition variable.
Clients_monitor  Clients_monitor_initialization(){
    Clients_monitor  monitor;
    if((monitor = malloc(sizeof(struct Clients_monitor))) == NULL){
        perror("malloc");
        return NULL;
    }

        // Initialize mutex with default attributes
    if (pthread_mutex_init(&monitor->lock, NULL) != 0) {
        perror("pthread_mutex_init");
        free(monitor);
        return NULL;
    }

    // Initialize condition variable with default attributes
    if (pthread_cond_init(&monitor->ready, NULL) != 0) {
        perror("pthread_cond_init");
        pthread_mutex_destroy(&monitor->lock);
        free(monitor);
        return NULL;
    }
    monitor->count = 0;
    return monitor;
}
// [Tracker] Increments the count of active clients.
void increase_clients(Clients_monitor  monitor){
    pthread_mutex_lock(&monitor->lock);
    monitor->count+=1;
    pthread_mutex_unlock(&monitor->lock);
}
// [Tracker] Decrements the client count. Signals condition if count reaches zero.
void decrease_clients(Clients_monitor  monitor){
    pthread_mutex_lock(&monitor->lock);
    monitor->count-=1;
    if(monitor->count == 0)
        pthread_cond_signal(&monitor->ready);

    pthread_mutex_unlock(&monitor->lock);
}
// [Destructor] Waits for all clients to disconnect, then cleans up the monitor.
void destroy_Clients_monitor(Clients_monitor monitor){

    pthread_mutex_lock(&monitor->lock);
    while(monitor->count != 0){   
        pthread_cond_wait(&monitor->ready , &monitor->lock);
    }

    pthread_mutex_unlock(&monitor->lock);
    
    pthread_mutex_destroy(&monitor->lock);
    pthread_cond_destroy(&monitor->ready);
    free(monitor);    
}

