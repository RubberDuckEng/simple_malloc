# simple_malloc
 Lets write a simple malloc

# Usage:
clang malloc.c && ./a.out

# Things to do
* Benchmark (space and time)
* Slab allocation / small object table
* Compress header
* Freelist (performance)
* Use out how to use as the real malloc for a program
  - Needs: malloc, calloc, free, realloc
  - LD_PRELOAD
* Tech to C++
* Return memory to the OS
* refactor free functions to be object oriented (e.g. node_set_next_free)