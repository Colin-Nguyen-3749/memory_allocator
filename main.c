#include <unistd.h>
#include <pthread.h>

//========================
// malloc
// This function allocates n bytes of memory 
// (where n represents size) and return a pointer
// to the allocated memory
// INPUT: size_t size (unsigned integer)
// OUTPUT: none
//=========================
void *malloc(size_t size) {
    
    size_t total_size;
    void *block;
    header_t *header;

    // Check if the requested size is 0; return NULL if so
    if (!size) return NULL;

    // Since our size if valid, acquire the lock
    pthread_mutex_lock(&global_malloc_lock);

    // This traverses the linked list to see if there's already
    // an existing block of memory marked as free and can accomodate
    // the given size
    header = get_free_block(size);

    // Okay, we found a sufficiently large-enough free block. Now,
    // the header pointer will refer to the header part of the block of 
    // memory that wsa just found by traversing the list.
    if (header) {
        header->s.is_free = 0;
        pthread_mutex_unlock(&global_malloc_lock);

        // header + 1 points to the byte right after the end of the header
        // This is also the first byte of the actual memory block, which is 
        // what the caller is interested in. Cast this to void and return.
        return (void*)(header + 1);
    }

    total_size = sizeof(header_t) + size;

    // If a sufficiently large enough free block isn't found, extend the heao
    // by calling sbrk(), which extends the heap by the size that fits the requested size 
    // plus the header (total_size) which was computed above.
    block = sbrk(total_size);

    if (block == (void*) -1) {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    header = block;
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = NULL;

    if (!head) head = header;

    if (!tail) tail->s.next = header;

    tail = header;
    pthread_mutex_unlock(&global_malloc_lock);

    return (void*)(header + 1);
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

// Head and tail pointer to keep track of the list
header_t *head, *tail;

// Prevent multiple threads from concurrently accessing memory
// Use a global lock so that before evey action you need to acquire the lock
// and after you've finished, release the lock
pthread_mutex_t global_malloc_lock;

// idk what this is they didn't explain this
header_t *get_free_block(size_t size) {

    header_t *curr = head;
    while (curr) {
        if (curr->s.is_free && curr->s.size >= size) {
            return curr;
        }

        curr = curr->s.next;
    }

    return NULL;
}