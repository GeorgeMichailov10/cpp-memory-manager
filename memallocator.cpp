#include <unistd.h>
#include <string.h>
#include <pthread.h>

typedef char ALIGN[16];

union header {
    struct {
        size_t size;
        unsigned is_free;
        union header *next;
    } s;
    ALIGN stub;
};
typedef union header header_t;

// Allocate a memory block big enough to fit size bytes.
void* malloc(size_t size) {
    void* mem_block = sbrk(size);
    if (mem_block == (void*) -1) {
        return nullptr;
    }
    return mem_block;
}

header_t *head, *tail;
pthread_mutex_t global_malloc_lock;

// Allocatoes a block of size size.
void* malloc(size_t size) {
    size_t total_size;
    void *block;
    header_t *header;

    if (!size) return nullptr;
    pthread_mutex_lock(&global_malloc_lock);

    
    header = get_free_block(size);
    if (header) {
        header->s.is_free = 0;
        pthread_mutex_unlock(&global_malloc_lock);
        return (void *)(header + 1);
    }

    total_size = sizeof(header_t) + size;
    block = sbrk(total_size);
    if (block == (void*) -1) {
        pthread_mutex_unlock(&global_malloc_lock);
        return nullptr;
    }

    header = (header_t *) block;
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = nullptr;
    
    if (!head) head = header;
    if (tail) tail->s.next = header;

    tail = header;
    pthread_mutex_unlock(&global_malloc_lock);
    return (void*)(header + 1);
}

// Finds the next free memory block or returns nullptr if not found.
header_t* get_free_block(size_t size) {
    header_t *curr = head;
    while (curr) {
        if (curr->s.is_free && curr->s.size >= size) {
            return curr;
        }
        curr = curr->s.next;
    }
    return nullptr;
}

// Free allocated memory.
void free(void* mem_block) {
    header_t *header, *temp;
    void *programbreak;

    if (!mem_block) return;

    pthread_mutex_lock(&global_malloc_lock);
    header = (header_t*)mem_block - 1;

    programbreak = sbrk(0);
    if ((char*)mem_block + header->s.size == programbreak) {
        if (head == tail) {
            head = tail = nullptr;
        } else {
            temp = head;
            while (temp) {
                if (temp->s.next == tail) {
                    temp->s.next = nullptr;
                    tail = temp;
                }
                temp = temp->s.next;
            }
        }
        sbrk(0 - sizeof(header_t) - header->s.size);
        pthread_mutex_unlock(&global_malloc_lock);
        return;
    }
}

// Memory allocator of an array of num elements of nsize bytes and returns a pointer.
void* calloc(size_t num, size_t nsize) {
    size_t size;
    void* block;

    // Ensure both are valid.
    if (!num || !nsize) {
        return nullptr;
    }

    // Determine amount of memory needed in bytes.
    size = num*nsize;

    // Check for overflow
    if (nsize != size / num) {
        return nullptr;
    }

    // Create memory block.
    block = malloc(size);

    if (!block) {
        return nullptr;
    }

    memset(block, 0, size);
    return block;
}

// Change the size of the given memory block to size param.
void* realloc(void* block, size_t size) {
    header_t* header;
    void* ret;

    if (!block || !size) {
        return malloc(size);
    }

    header = (header_t*)block-1;

    if (header->s.size >= size) {
        return block;
    }

    ret = malloc(size);
    if (ret) {
        memcpy(ret, block, header->s.size);
        free(block);
    }
    return ret;
}