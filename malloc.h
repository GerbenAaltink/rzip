#ifndef RZIP_MALLOC_H
#define RZIP_MALLOC_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

unsigned int rzip_malloc_count = 0;
unsigned int rzip_malloc_alloc_count = 0;
unsigned int rzip_malloc_free_count = 0;

void *rzip_malloc(size_t size)
{
    rzip_malloc_count++;
    rzip_malloc_alloc_count++;
    return malloc(size);
}
void *rzip_realloc(void *obj, size_t size)
{
    return realloc(obj, size);
}
void *rzip_free(void *obj)
{
    rzip_malloc_count--;
    rzip_malloc_free_count++;
    free(obj);
    return NULL;
}

char *rzip_malloc_stats()
{
    static char res[100] = {0};
    sprintf(res, "Memory usage: %d allocated, %d freed, %d in use.", rzip_malloc_alloc_count, rzip_malloc_free_count, rzip_malloc_count);
    return res;
}

char *rzip_strdup(char *str)
{

    char *res = (char *)strdup(str);
    rzip_malloc_alloc_count++;
    rzip_malloc_count++;
    return res;
}

#endif