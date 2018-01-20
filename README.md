# Allocators
Just working on different kinds of dynamic memory allocators

These allocators serve as usable modules that conform to the C standards for
malloc, free, calloc, and realloc. The idea is that these can serve as building
blocks for "the" memory allocator of a system.

Example use case: running baremetal on a microcontroller, you can implement
your system-wide malloc()/free()/etc. with a global knuth state and calls
to the knuth\_\* functions.

