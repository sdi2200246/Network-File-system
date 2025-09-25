#include "clients_threads.h"

// [Helper] Sends a header message to the client indicating the result of an operation.
// - On success: sends "OK <size>"
// - On error: pops an error message from Errors and sends "ERROR <length> <message>"
void write_header_answer_message(int sock , Vector Errors , int status , int size){
    
    char message[MESSAGESHOLDER]= "";   
    char *Error_message; 

    //OKAY STATUS
    if(status == 0){
        snprintf(message , sizeof(message) , "OK %d " , size);
        if(write(sock , message , strlen(message)) <= 0){
            printf("write_header_answer:%s" ,strerror(errno));
        };
    }
    //ERROR STATUS.
    else{
        Error_message = (char*)Errors->pop(Errors);
        snprintf(message , sizeof(message) , "ERROR %d %s " ,(int)strlen(Error_message) , Error_message);
        if(write(sock , message , strlen(message)) <= 0){
            printf("write_header_answer:%s" ,strerror(errno));
        }
    }
}
// [Core] Handles a LIST request from the client.
// - Opens the specified directory
// - Reads file names (skipping hidden files)
// - Constructs a response message with filenames
// - Sends it to the client preceded by a header
int List(int sock , const char * source_dir , Vector Errors){

    DIR *dir;
    struct dirent *entry;
    Vector files;
    int bytes = 2;
    char *file , *List_message;

    if((dir = opendir(source_dir)) == NULL){
        printf("Error occured.");
        Errors->push_back(Errors , strdup(strerror(errno)));
        return -1 ;  
    }
    if((files = vector_create(1)) == NULL){
        Errors->push_back(Errors , strdup(strerror(errno)));
        return -1 ;
    }

    //save the names of the files in a vector while calculatting the size of the List message answer. (file's len + endofline).
    while((entry = readdir(dir)) != NULL){
        if(entry->d_name[0] =='.')
            continue;
        bytes += strlen(entry->d_name)+1;
        files->push_back(files , strdup(entry->d_name));
    }

    if((List_message = malloc(sizeof(char)*bytes)) == NULL){
        Errors->push_back(Errors , strdup(strerror(errno)));
        return -1 ;
    }

    int counter = 0;
    //Start creation of List_message.
    while(files->size){
        file = (char*)files->pop(files);
        for(int i = 0; i < strlen(file) ; i++){
            List_message[counter] = file[i];
            counter+=1;
        }
        List_message[counter++] = '\n';
        free(file);
    }

    List_message[bytes-2]='.'; List_message[bytes-1] = '\0';
    //End of creation.
    
    //send the requested data to the client.
    write_header_answer_message(sock , Errors , 0 , bytes-1);
    
    if(write(sock , List_message , bytes) == -1){
        files->destroy(files);
        free(List_message);
        closedir(dir);
        Errors->push_back(Errors , strdup(strerror(errno)));
        return -1;
    }
    //free resources.
    free(List_message);
    files->destroy(files);
    closedir(dir);
    return 1;
}

// [Core] Handles a PULL request from the client.
// - Opens the specified file and reads it in chunks into a buffer
// - Sends a header with file size
// - Transmits the file data in full to the client
int Pull(int sock , const char* target_path , Vector Errors){

    int target_fd, bytes_read;
    char* buffer = NULL;
    size_t total_size = 0;
    size_t buffer_capacity = 0;

    // Open the file
    if ((target_fd = open(target_path, O_RDONLY)) == -1) {
        Errors->push_back(Errors, strdup(strerror(errno)));
        close(sock);
        return -1;
    }

    // Read the file in chunks
    while (1) {
        // Ensure there is enough space
        if (total_size +  MAX_DATA_CAPACITY > buffer_capacity) {
            buffer_capacity += MAX_DATA_CAPACITY;
            char* temp = realloc(buffer, buffer_capacity);
            if (!temp) {
                free(buffer);
                Errors->push_back(Errors, strdup("Memory allocation failed"));
                close(target_fd);
                close(sock);
                return -1;
            }
            buffer = temp;
        }
        // Read into the buffer
        bytes_read = read(target_fd, buffer + total_size, MAX_DATA_CAPACITY);
        if (bytes_read < 0) {
            Errors->push_back(Errors, strdup(strerror(errno)));
            free(buffer);
            close(target_fd);
            close(sock);
            return -1;
        } else if (bytes_read == 0) {
            break;  // End of file
        }

        total_size += bytes_read;
    }

    close(target_fd);

    // Send header
    write_header_answer_message(sock, Errors, 0, total_size);

    // Write the whole buffer to socket
    size_t total_written = 0;
    while (total_written < total_size) {
        ssize_t bytes_written = write(sock, buffer + total_written, total_size - total_written);
        if (bytes_written < 0) {
            Errors->push_back(Errors, strdup(strerror(errno)));
            free(buffer);
            close(sock);
            return -1;
        }
        total_written += bytes_written;
    }

    free(buffer);
    return 1;
}
// [Core] Handles a PUSH request from the client in three stages:
// - chunksize == -1: opens and truncates the file
// - chunksize > 0: writes the provided chunk of data to the file
// - chunksize == 0: closes the file descriptor
int Push(int sock , char *file , int chunksize , void *data , int targetfd , Vector Errors){

    int target_fd , written_bytes;
    //case of just opnening and truncating.
    if(chunksize == -1){
        if((target_fd = open(file , O_WRONLY | O_CREAT | O_TRUNC)) == -1){
            Errors->push_back(Errors , strdup(strerror(errno)));
            return -1;
        }
        write_header_answer_message(sock , Errors , 0 , 0);
        return target_fd;
    }
    //case of appending data.
    if(chunksize > 0){
        if(targetfd == -1){
            Errors->push_back(Errors , strdup("Wrong push sequence."));
            return -1;
        }

        if((written_bytes = write(targetfd , data , chunksize)) == -1){
            Errors->push_back(Errors , strdup(strerror(errno)));
            return -1;
      }
      write_header_answer_message(sock , Errors , 0 , 0);
      return targetfd;
    }
    //case of closing the file.
    if(chunksize == 0){
        if((close(targetfd)) == -1){
            Errors->push_back(Errors , strdup(strerror(errno)));
            return -1;
        }
        write_header_answer_message(sock , Errors , 0 , 0);   
        return 1;
    }
    //case of closing the file
    return 1;
   
}

// [Helper] Parses a request string from the client into:
// - operation (op), filename (source_file), and chunk size string (size)
// Converts chunk size to integer for PUSH operations.
int read_request(int sock , char* request , char*op ,char*source_file , char*size , int *chunk_size , Vector Errors){
    int bytes_read , args;

    if((bytes_read = read(sock , request , MAX_DATA_CAPACITY)) == -1){
        Errors->push_back(Errors , strdup(strerror(errno)));
        return -1;
    }
    if(bytes_read > 4*MESSAGESHOLDER){
        Errors->push_back(Errors , strdup("Uknown error"));
        return -1;
    }
    request[bytes_read] = '\0';
    if((args = sscanf(request, "%15s %500s %33s", op, source_file, size)) == -1){
        Errors->push_back(Errors , strdup("Uknown error"));
        return -1;
    }
    if(size == NULL){
        Errors->push_back(Errors , strdup("Uknown error"));
        return -1;
    }

    if(!strcmp("PUSH" , op))
       *chunk_size = atoi(size);
   
    return 1;
}
// [Helper] Reads the chunk size portion from a PUSH request header.
// - Waits until it passes 3 space-separated fields and extracts the chunk size
int read_Push_request_chunk_size(int sock , Vector Errors){

    int bytes_read , count = 0 , cindex = 0;
    char request[2] ="" , chunk[MAX_PACKAGE_SIZE] = "";

    //count the  spaces until you find the chunk part of the Push message.
    while(count < 3){
        if((bytes_read = read(sock , request , 1)) == -1){
            Errors->push_back(Errors , strdup(strerror(errno)));
            return -1;
        }
         if(!strcmp(request , " ")){
            count+=1;
            continue;
        }
        if(cindex > MAX_PACKAGE_SIZE){
            Errors->push_back(Errors , strdup("Unexpected error\n"));
            return -1;
        }

        //found the chunk part.
        if(count == 2)
            chunk[cindex++] = request[0];
    }
    chunk[cindex] = '\0';
    return atoi(chunk);
}

// [Handler] Main client handler thread function.
// - Reads and parses client requests (LIST, PULL, PUSH)
// - Dispatches to the appropriate core function
// - Manages PUSH sessions with chunked writes
// - Handles all error reporting and resource cleanup
void* handle_client(void* arg){

    int sock = *((int*)arg) , bytes_read , chunk_size , targetfd = -1;
    char request[MESSAGESHOLDER*4]="" , op[MESSAGESHOLDER]="" , source_file[MAXFILENAME] ="" , size[MAX_PACKAGE_SIZE+1] = ""  , *data = NULL; 
    Vector Errors = vector_create(1);
    //read clients request.
    read_request(sock , request , op , source_file , size , &chunk_size , Errors);
    //call List function for LIST request.
    if(!strcmp("LIST" , op ) && Errors->is_empty(Errors))
        List(sock , source_file , Errors);
    
    //handle series of Push requests for the same target.
    if(!strcmp("PUSH" , op) && Errors->is_empty(Errors)){
        //first read the -1 chunksize wich is the first one
        targetfd = Push(sock ,source_file , chunk_size ,  NULL , -1 , Errors);
        
        while(Errors->is_empty(Errors) && chunk_size != 0){
            //read the protocol to find the size of the chunk.
            if((chunk_size = read_Push_request_chunk_size(sock , Errors)) == -1)
                break;
            //read the data all at once (of one Push request).
            if((data = malloc(sizeof(char)*(chunk_size+1))) == NULL){
                Errors->push_back(Errors , strdup(strerror(errno)));
                break;
            }
            //check you got ther data.
            if(chunk_size && (bytes_read = read(sock , data , chunk_size)) == -1){
                Errors->push_back(Errors , strdup(strerror(errno)));
                break;
            }
            //write the data of the Push requestall at once.
            if((Push(sock , source_file , chunk_size , data , targetfd , Errors)) == -1)
                break;
                
            free(data);
        }
    }
    //call Pull function for PULL request.
    if(!strcmp("PULL" , op) && Errors->is_empty(Errors))
        Pull(sock , source_file , Errors);
        
    //CLEAN UP.
    //soemthing went wrong in the push function.
    if(targetfd != -1){
        close(targetfd);
    }
    //Error occured
    if(!Errors->is_empty(Errors)){
        write_header_answer_message(sock , Errors , 1 , 0);
    }

    close(sock);
    free(arg);
    Errors->destroy(Errors);
    pthread_exit(NULL);
}
