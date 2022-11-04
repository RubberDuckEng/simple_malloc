#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

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
    write(STDERR_FILENO, "heap initialized\n", 17);
}

// allocator -------------------------------------------------------------------

#define ALIGN8(n) (((n) + 8 - 1) & ~(8 - 1))

static void my_log(const char *msg, size_t value)
{
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "%s(%zu)\n", msg, value);
    write(STDERR_FILENO, buffer, strlen(buffer));
}

// allocates a block of memory
// returns 0 on failure.
void *
malloc(size_t requested_size)
{
    my_log("malloc", requested_size);
    if (g_pool.start == NULL)
    {
        init_heap();
    }
    if (requested_size <= 0)
    {
        return NULL;
    }
    struct node *prev_free = NULL;
    struct node *current = g_pool.first_free;
    while (current != NULL)
    {
        assert(current->is_free);
        if (current->size >= requested_size)
        {
            // Free nodes must be large enough to fit both header and body.
            // Otherwise it's left as fragmented space which will be
            // reclaimed when the section before is freed.

            // Goal: Every "client region" is 8-byte aligned (i.e., starts at an
            // 8 byte offset).
            // Strategy: Always align nodes at 8-byte boundaries.
            static_assert(sizeof(struct node) == ALIGN8(sizeof(struct node)), "node must a multiple of 8-bytes");

            // Starting with:
            // | node | free space |

            // Question: If we allocated the client's request from the current
            // node's free-space, would there be enough left over to create
            // a new node in the free space?
            // e.g. Do we want to create:
            // | node | client region | free space for alignment | node | free space |
            // Or do we just do:
            // | node | client region | free space |

            // Sum of "client region" and "free space for alignment"
            const size_t aligned_size = ALIGN8(requested_size);
            // Sum of "node" and the least "free space" we're interested in:
            const size_t kMinimumFreeNodeSize = sizeof(struct node) + sizeof(struct body);

            if (current->size > aligned_size + kMinimumFreeNodeSize)
            {
                // Split the node.
                struct node *new_node = (struct node *)((char *)current + aligned_size);
                node_insert_after(current, new_node);
                new_node->size = current->size - aligned_size - sizeof(struct node);
                current->size = aligned_size;
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
void free(void *ptr)
{
    if (ptr == NULL)
        return;
    // find where the free node would go in the free list?
    // make a new node for it?
    struct node *node = node_from_ptr(ptr);
    my_log("free", node->size);
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
void *calloc(size_t number_of_members, size_t size)
{
    const size_t number_of_bytes = number_of_members * size;
    my_log("calloc", number_of_bytes);
    void *ptr = malloc(number_of_bytes);
    // TODO: Rather than touching every page, we should instead realize that
    // the pages are already zeroed when we get them from the OS.
    memset(ptr, 0, number_of_bytes);
    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    my_log("realloc", size);
    // TODO: Check whether the node is large enough to fit the new size and just
    // update the metadata rather than always allocating a new node.
    void *new_ptr = malloc(size);
    memcpy(new_ptr, ptr, size);
    free(ptr);
    return new_ptr;
}
