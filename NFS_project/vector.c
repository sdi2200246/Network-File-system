#include "vector.h"
#include "objects.h"


Vector vector_create(int initial_capacity) {
    Vector vec = malloc(sizeof(struct Vector));
    if (!vec) {
        perror("vector_create: malloc failed for Vector struct");
        return NULL;
    }   
    vec->size = 0;
    vec->capacity = initial_capacity > 0 ? initial_capacity : VECTOR_INIT_CAPACITY;
    
    vec->data = malloc(vec->capacity * sizeof(void*));
    if (!vec->data) {
        perror("vector_create: malloc failed for data array");
        free(vec);
        return NULL;
    }
    vec->push_back = vector_push_back;
    vec->delete_back = vector_delete_back;
    vec->delete_at = vector_delete_at;
    vec->at = vector_at;
    vec->get_size = vector_size;
    vec->get_capacity = vector_capacity;
    vec->is_empty = vector_empty;
    vec->pop = vector_pop;
    vec->clear = vector_clear;
    vec->destroy = vector_destroy;
    return vec;
}
void vector_destroy(Vector vec){
    
    if (!vec) {
        fprintf(stderr, "vector_destroy: NULL vector pointer\n");
        return;
    }
    if(vec->size)vector_clear(vec);
    free(vec->data);
    free(vec);
}

void vector_push_back(Vector vec, void* element){

    if (!vec) {
        fprintf(stderr, "vector_push_back: NULL vector pointer\n");
        return;
    }
    
    if (!element) {
        fprintf(stderr, "vector_push_back: NULL element pointer\n");
        return;
    }
    
    if (vec->size >= vec->capacity) {
        int new_capacity = vec->capacity * VECTOR_GROWTH_FACTOR;
        void** new_data = realloc(vec->data, new_capacity * sizeof(void*));
        
        if (!new_data) {
            perror("vector_push_back: realloc failed");
            return;
        }
        vec->data = new_data;
        vec->capacity = new_capacity;
    }
    vec->data[vec->size++] = element;
}

void vector_delete_back(Vector vec) {
    if (!vec){ 
        fprintf(stderr, "vector_pop_back: NULL vector pointer\n");
        return;
    }
    if (vector_empty(vec)){ 
        fprintf(stderr, "vector_pop_back: empty vector\n");
        return;
    }
    free(vec->data[--vec->size]);
}

void vector_delete_at(Vector vec , int index){
    if (!vec){
        fprintf(stderr, "vector_pop_back: NULL vector pointer\n");
        return;
    }

    if (vector_empty(vec)){
        fprintf(stderr, "vector_pop_back: empty vector\n");
        return;
    }

    if(index < 0 || index>= vec->size){
        fprintf(stderr, "vector_pop_back: empty vector\n");
        return;
    }
    for(int i = index+1; i < vec->size ; i++)
        vec->data[i-1] = vec->data[i];

    vec->size--;
    return;
    
}
void* vector_at(Vector vec, int index) {
    if (!vec) {
        fprintf(stderr, "vector_at: NULL vector pointer\n");
        return NULL;
    }
    if (index >= vec->size) {
        fprintf(stderr, "vector_at: index out of bounds (%d >= %d)\n", index, vec->size);
        return NULL;
    }
    return vec->data[index];
}
int vector_size(Vector vec) {
    if (!vec) {
        fprintf(stderr, "vector_size: NULL vector pointer\n");
        return 0;
    }
    return vec->size;
}
int vector_capacity(Vector vec) {
    if (!vec) {
        fprintf(stderr, "vector_capacity: NULL vector pointer\n");
        return 0;
    }
    return vec->capacity;
}
int vector_empty(Vector vec) {
    if (!vec) {
        fprintf(stderr, "vector_empty: NULL vector pointer\n");
        return -1;
    }
    return vec->size == 0;
}

void* vector_pop(Vector vec){
    if (!vec){
        fprintf(stderr, "vector_pop: NULL vector pointer\n");
        return NULL;
    }
    if(!vec->size){
        fprintf(stderr, "vector_pop: empty vector\n");
        return NULL;
    }
    void* data = vec->data[0];
    for(int i = 1; i < vec->size ; i++)
        vec->data[i-1] = vec->data[i];
    vec->size--;
    return data;
}
void vector_clear(Vector vec){
    if (!vec){
        fprintf(stderr, "vector_clear: NULL vector pointer\n");
        return;
    }
    for(int i = 0 ; i < vec->size ; i++){
        if(vec->data[i] != NULL)
            free(vec->data[i]);
    }
    vec->size = 0;
}

int *intdup(int value) {
    int *ptr = malloc(sizeof(int));
    if (ptr) *ptr = value;
    return ptr;
}

