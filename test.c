#include "malloc.h"

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
