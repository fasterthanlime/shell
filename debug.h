#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define DEBUG 1

// string handling in C suxxorz
#define dbg(fmt, ...) \
    do { if(DEBUG) { \
        char __s1[2059]; \
        char __s2[2049]; \
        snprintf(__s1, 10, "[%d] ", getpid()); \
        snprintf(__s2, 2048, fmt, __VA_ARGS__); \
        strncat(__s1, __s2, 2048); \
        fprintf(stderr, "%s", __s1); \
    }} while(0)

#endif // __DEBUG_H__
