#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/mman.h>

// page ------------------------------------------------------------------------

static void *page_alloc(size_t size)
{
    if (size <= 0)
    {
        return NULL;
    }
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
    {
        return NULL;
    }
    return ptr;
}

// node ------------------------------------------------------------------------

struct node
{
    struct node *next; // next node in memory order.
    struct node *prev; // previous node in memory order.
    size_t size;       // Does not include size of node.
    bool is_free;
};

struct body
{
    struct node *next_free; // Set if free, not in memory order.
};

static struct body *node_get_body(struct node *node)
{
    return (struct body *)((char *)node + sizeof(struct node));
}

static struct node *node_get_next_free(struct node *node)
{
    return node_get_body(node)->next_free;
}

static void node_set_next_free(struct node *node, struct node *next_free)
{
    node_get_body(node)->next_free = next_free;
}

static void node_insert_after(struct node *left, struct node *right)
{
    right->next = left->next;
    right->prev = left;
    left->prev = right;
}

static struct node *node_from_ptr(void *ptr)
{
    return (struct node *)((char *)ptr - sizeof(struct node));
}

static void *node_get_ptr(struct node *node)
{
    return (void *)((char *)node + sizeof(struct node));
}

// pool ------------------------------------------------------------------------

static struct pool
{
    void *start;
    size_t size;
    struct node *first;
    struct node *first_free;
} g_pool = {NULL, 0};

void init_heap(void)
{
    const size_t pool_size = ((size_t)10) * 1024 * 1024 * 1024; // 10 GB
    g_pool.start = page_alloc(pool_size);
    g_pool.size = pool_size;
    g_pool.first = (struct node *)g_pool.start;
    g_pool.first->next = NULL;
    g_pool.first->prev = NULL;
    g_pool.first->size = g_pool.size - sizeof(struct node);
    g_pool.first->is_free = true;
    node_set_next_free(g_pool.first, NULL);
    g_pool.first_free = g_pool.first;
}

// allocator -------------------------------------------------------------------

// allocates a block of memory
// returns 0 on failure.
void *
my_malloc(size_t size)
{
    if (size <= 0)
    {
        return NULL;
    }
    struct node *prev_free = NULL;
    struct node *current = g_pool.first_free;
    while (current != NULL)
    {
        assert(current->is_free);
        if (current->size >= size)
        {
            // Free nodes must be large enough to fit both header and body.
            // Otherwise it's left as fragmented space which will be
            // reclaimed when the section before is freed.
            const size_t kMinimumFreeNodeSize = sizeof(struct node) + sizeof(struct body);
            if (current->size > size + kMinimumFreeNodeSize)
            {
                // Split the node.
                struct node *new_node = (struct node *)((char *)current + sizeof(struct node) + size);
                node_insert_after(current, new_node);
                new_node->size = current->size - size - sizeof(struct node);
                current->size = size;
                new_node->is_free = true;

                node_set_next_free(new_node, node_get_next_free(current));
                if (prev_free != NULL)
                {
                    // Insert into the free list after prev_free.
                    node_set_next_free(prev_free, new_node);
                }
                else
                {
                    // The first free node had enough space, make this first.
                    g_pool.first_free = new_node;
                }
            }
            else
            {
                struct node *next_free = node_get_next_free(current);
                if (prev_free != NULL)
                {
                    node_set_next_free(prev_free, next_free);
                }
                else
                {
                    g_pool.first_free = next_free;
                }
            }
            current->is_free = false;
            return node_get_ptr(current);
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
    if (ptr == NULL)
        return;
    // find where the free node would go in the free list?
    // make a new node for it?
    struct node *node = node_from_ptr(ptr);
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
    else
    {
        node_set_next_free(node, g_pool.first_free);
        g_pool.first_free = node;
    }
    // TODO: We could merge with the next node if it's free, but we don't know
    // how to maintain the free list in that case.  If we made the free list
    // doubly linked, then we could do it.
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
