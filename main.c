#include <unistd.h>

//========================
// malloc
// This function allocates n bytes of memory 
// (where n represents size) and return a pointer
// to the allocated memory
// INPUT: size_t size (unsigned integer)
// OUTPUT: none
//=========================
void *malloc(size_t size) {
    
    void *block;
    block = sbrk(size);
    // On failure, sbrk() returns (void*) -1
    if (block == (void*) -1) {
        return NULL;
    };

    // If success, n bytes (n = size) are allocated on the heap
    return block;
}

//==========================
// header_t
// This struct tracks the size of a block of memory
// and whether or not it is free. We need size because 
// when we have to free the memory pointed to by ptr, we 
// need to know the size of what we're freeing.
// Heap memory is contiguous, and we can only release memory 
// that's at the end of the heap. However, freeing and releasing
// memory are two different things. Freeing memory doesn't release
// it back to the OS, but rather it's just marked as free and can 
// be reused in a later malloc() call. 
//============================

typedef char ALIGN[16];

union header {
    struct {
        size_t size;
        unsigned is_free;

        // Use a linked list to keep track of the memory
        struct header_t *next;
    } s;
    ALIGN stub;
};

typedef union header header_t;