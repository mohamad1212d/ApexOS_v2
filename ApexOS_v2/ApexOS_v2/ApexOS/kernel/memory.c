/* =====================================================
   kernel/memory.c - Simple heap allocator + utils
===================================================== */
#include "apex.h"

/* Heap starts at 4MB */
static uint8_t *heap = (uint8_t*)0x400000;
static size_t   heap_used = 0;
static uint32_t total_mem_kb = 0;

void pmm_init(uint32_t mem_kb){
    total_mem_kb = mem_kb;
}

void *kmalloc(size_t sz){
    sz = (sz + 15) & ~15u; /* 16-byte align */
    void *ptr = heap + heap_used;
    heap_used += sz;
    return ptr;
}

void kfree(void *ptr){ (void)ptr; /* simple bump allocator */ }

void *kmemset(void *dst, int c, size_t n){
    uint8_t *p=dst;
    while(n--) *p++=(uint8_t)c;
    return dst;
}

void *kmemcpy(void *dst, const void *src, size_t n){
    uint8_t *d=dst; const uint8_t *s=src;
    while(n--) *d++=*s++;
    return dst;
}

size_t kstrlen(const char *s){
    size_t n=0; while(*s++) n++; return n;
}

int kstrcmp(const char *a, const char *b){
    while(*a&&*a==*b){a++;b++;}
    return (uint8_t)*a-(uint8_t)*b;
}

int kstrncmp(const char *a, const char *b, size_t n){
    while(n&&*a&&*a==*b){a++;b++;n--;}
    if(!n) return 0;
    return (uint8_t)*a-(uint8_t)*b;
}

char *kstrcpy(char *dst, const char *src){
    char *r=dst; while((*dst++=*src++)); return r;
}

char *kstrcat(char *dst, const char *src){
    char *r=dst;
    while(*dst) dst++;
    while((*dst++=*src++));
    return r;
}
