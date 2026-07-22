#include <unistd.h>
#include <pthread.h>

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

// This searches through the linked list to find a block that's
// been marked as 'free' and is the right size for what we want
// If it's free, then return the location, if not, return NULL
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

//========================
// malloc
// This function allocates n bytes of memory 
// (where n represents size of what you want to store)
// and return a pointer to the allocated memory
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
    // memory that was just found by traversing the list.
    if (header) {
        header->s.is_free = 0;
        pthread_mutex_unlock(&global_malloc_lock);

        // header + 1 points to the byte right after the end of the header
        // This is also the first byte of the actual memory block, which is 
        // what the caller is interested in. Cast this to void and return.
        return (void*)(header + 1);
    }

    total_size = sizeof(header_t) + size;

    // If a sufficiently large enough free block isn't found, extend the heap
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

//======================================
// free()
// This function has to first figure out if the 
// block that's going to be freed is at the end of 
// the heap or not. If it is, then it can be released
// back to the OS. If not, then just mark it as 'free'
// and then hope that we can reuse it later.
//======================================
void free(void *block) {
    
    header_t *header, *tmp;
    void *programbreak;

    if (!block) return;

    pthread_mutex_lock(&global_malloc_lock);

    // This gets the header of the block that we want to free
    // We want to get a pointer that's behind the block by a distance 
    // that equals the size of the header (which is why we subtract 1 below)
    header = (header_t*)block - 1;

    // This gives the current value of the program break
    programbreak = sbrk(0);

    // This searches for the end of the current block to check if the 
    // block that we want to free is at the end of the heap
    // (It's just parsing through the linked list, nothing more)
    if ((char*)block + header->s.size == programbreak) {
        if (head == tail) {
            head = tail = NULL;
        } else {
            tmp = head;
            while (tmp) {
                if (tmp->s.next == tail) {
                    tmp->s.next = NULL;
                    tail = tmp;
                }
                tmp = tmp->s.next;
            }
        }

        // If we are at the end, now we can shrink the size of the heap and 
        // release memory back to the OS.
        // Head and tail pointers are reset to reflect the loss of the last block.
        // The amount of memory to be released is also calculated
        // which is done by calling sbrk with the negative of this value (which is 
        // why we subtract all of it from 0).
        sbrk(0 - sizeof(header_t) - header->s.size);
        pthread_mutex_unlock(&global_malloc_lock);

        return;
    }

    // If the block is not the last one in the linked list, we set the is_free
    // field to its header.
    header->s.is_free = 1;
    pthread_mutex_unlock(&global_malloc_lock);
}

//====================================
// calloc()
// This function allocates memory for an array of 
// n number of elements of m size bytes each and returns 
// a pointer to the allocated memory. 
// Additionally, the memory is all set to zeroes.
// INPUT: size_t num, size_t nsize
// OUTPUT: a pointer to the allocated memory
//=====================================
void *calloc(size_t num, size_t nsize) {

    size_t size;
    void *block;

    // If num or nsize is invalid
    if (!num || !nsize) return NULL;

    size = num * nsize;
    
    // Check multiplicative overflow
    if (nsize != size / num) return NULL;
    block = malloc(size);
    
    if (!block) return NULL;
    
    // Clears the allocated memory to all zeroes
    memset(block, 0, size);

    return block;
}

//====================================
// realloc()
// This function changes the size of the given 
// memory block to the size given
// INPUT: void *block, size_t size
// OUTPUT: A new size for the memory block or  
// the block of the same size
//=====================================
void *realloc(void *block, size_t size) {

    header_t *header;
    void *ret;

    if (!block || !size) return malloc(size);
    
    // If the block already has the size to accomodate
    // the requested size, then there's nothing to be done
    header = (header_t*)block - 1;
    if (header->s.size >= size) return block;

    // If the block does not have the requested size, then call
    // malloc() to get a block of the request size, and relocate contents
    // to the new bigger block using memcpy(). 
    // The old memory block is then freed.
    ret = malloc(size);
    if (ret) {
        memcpy(ret, block, header->s.size);
        free(block);
    }

    return ret;
}