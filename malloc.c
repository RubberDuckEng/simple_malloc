#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>

//    void *mmap(void *addr, size_t length, int prot, int flags,
//               int fd, off_t offset);

void *page_alloc(size_t size)
{
    if (size <= 0)
        return NULL;
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
    {
        return NULL;
    }
    return ptr;
}

struct node
{
    struct node *next;
    struct node *prev;
    size_t size; // Does not include size of node.
    bool is_free;
};

static struct pool
{
    void *start;
    size_t size;
    struct node *first;
} g_pool = {NULL, 0};

void init_heap(void)
{
    const size_t pool_size = 10 * 1024 * 1024;
    g_pool.start = page_alloc(pool_size);
    g_pool.size = pool_size;
    g_pool.first = (struct node *)g_pool.start;
    g_pool.first->next = NULL;
    g_pool.first->prev = NULL;
    g_pool.first->size = g_pool.size - sizeof(struct node);
    g_pool.first->is_free = true;
}

// allocates a block of memory
// returns 0 on failure.
void *
my_malloc(size_t size)
{
    if (size <= 0)
        return NULL;
    struct node *current = g_pool.first;
    while (current != NULL)
    {
        if (current->is_free && current->size >= size)
        {
            if (current->size > size + sizeof(struct node))
            {
                // Split the node.
                struct node *new_node = (struct node *)((char *)current + sizeof(struct node) + size);
                new_node->next = current->next;

                new_node->prev = current;
                new_node->size = current->size - size - sizeof(struct node);
                new_node->is_free = true;
                current->next = new_node;
                current->size = size;
            }
            current->is_free = false;
            return (void *)((char *)current + sizeof(struct node));
        }
        current = current->next;
    }
    return NULL;
}

// Tells the system you're done with this block of memory.
// 0 is not an error.
// calling free twice is an error.
void my_free(void *ptr)
{
    // find where the free node would go in the free list?
    // make a new node for it?
    struct node *node = (struct node *)((char *)ptr - sizeof(struct node));
    node->is_free = true;
    if (node->prev && node->prev->is_free)
    {
        struct node *prev = node->prev;
        prev->size += node->size + sizeof(struct node);
        prev->next = node->next;
        if (node->next)
        {
            node->next->prev = prev;
        }
        node = prev;
    }
    if (node->next && node->next->is_free)
    {
        struct node *next = node->next;
        node->size += next->size + sizeof(struct node);
        node->next = next->next;
        if (next->next)
        {
            next->next->prev = node;
        }
    }
}

void dump_heap(void)
{
    // walk the free list and print out the free nodes.
    struct node *current = g_pool.first;
    while (current != NULL)
    {
        printf("Node: %p, size: %zu, is_free: %d\n", current, current->size, current->is_free);
        current = current->next;
    }
}

// clearing allocator, memory is always zeroed.
void *my_calloc(size_t number_of_members, size_t size);

void *my_realloc(void *ptr, size_t size);

int main(void)
{
    init_heap();
    dump_heap();
    void *ptr1 = my_malloc(100);
    printf("malloc(100) -> %p\n", ptr1);
    dump_heap();
    void *ptr2 = my_malloc(100);
    void *ptr3 = my_malloc(210);
    void *ptr4 = my_malloc(816);
    printf("More allocs\n");
    dump_heap();
    printf("free ptr2\n");
    my_free(ptr2);
    dump_heap();
    printf("free ptr3\n");
    my_free(ptr3);
    dump_heap();
    return 0;
}
