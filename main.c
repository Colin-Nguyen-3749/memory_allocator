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

    return block;
}