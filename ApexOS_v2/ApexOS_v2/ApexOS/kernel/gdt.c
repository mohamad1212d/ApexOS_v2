/* =====================================================
   kernel/gdt.c - Global Descriptor Table
===================================================== */
#include "apex.h"

typedef struct __attribute__((packed)){
    uint16_t limit_lo, base_lo;
    uint8_t  base_mid, access, gran, base_hi;
} gdt_entry_t;

typedef struct __attribute__((packed)){
    uint16_t limit; uint32_t base;
} gdt_ptr_t;

static gdt_entry_t gdt[5];
static gdt_ptr_t   gdtp;

extern void gdt_flush(uint32_t);

static void set(int i,uint32_t base,uint32_t lim,uint8_t acc,uint8_t gr){
    gdt[i].base_lo =(uint16_t)(base&0xFFFF);
    gdt[i].base_mid=(uint8_t)((base>>16)&0xFF);
    gdt[i].base_hi =(uint8_t)((base>>24)&0xFF);
    gdt[i].limit_lo=(uint16_t)(lim&0xFFFF);
    gdt[i].gran    =(uint8_t)(((lim>>16)&0x0F)|(gr&0xF0));
    gdt[i].access  =acc;
}

void gdt_init(void){
    set(0,0,0,0x00,0x00);
    set(1,0,0xFFFFF,0x9A,0xCF);
    set(2,0,0xFFFFF,0x92,0xCF);
    set(3,0,0xFFFFF,0xFA,0xCF);
    set(4,0,0xFFFFF,0xF2,0xCF);
    gdtp.limit=(uint16_t)(sizeof(gdt)-1);
    gdtp.base =(uint32_t)(uintptr_t)&gdt;
    gdt_flush((uint32_t)(uintptr_t)&gdtp);
}
