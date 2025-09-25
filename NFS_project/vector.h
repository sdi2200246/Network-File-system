#ifndef VECTOR_H
#define VECTOR_H

#include<string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define VECTOR_INIT_CAPACITY 4
#define VECTOR_GROWTH_FACTOR 2

typedef struct Vector* Vector;

struct Vector {
    void** data;      // Pointer to array of void pointers
    int size;         // Current number of elements
    int capacity;     // Total allocated capacity

    // Method-like function pointers
    void (*push_back)(Vector, void*);
    void (*delete_back)(Vector);
    void (*delete_at)(Vector, int);
    void* (*at)(Vector, int);
    int (*get_size)(Vector);
    int (*get_capacity)(Vector);
    int (*is_empty)(Vector);
    void* (*pop)(Vector);
    void (*clear)(Vector);
    void (*destroy)(Vector);
};

// Constructor
Vector vector_create(int initial_capacity);
void vector_destroy(Vector vec);
void vector_push_back(Vector vec, void* element);
void vector_delete_back(Vector vec);
void vector_delete_at(Vector vec, int index);
void* vector_at(Vector vec, int index);
int vector_size(Vector vec);
int vector_capacity(Vector vec);
int vector_empty(Vector vec);
void* vector_pop(Vector vec);
void vector_clear(Vector vec);
int *intdup(int value);
#endif // VECTOR_H
