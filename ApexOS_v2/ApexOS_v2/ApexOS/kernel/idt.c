/* =====================================================
   kernel/idt.c - Interrupt Descriptor Table + PIC
===================================================== */
#include "apex.h"

typedef struct __attribute__((packed)){
    uint16_t base_lo,sel;
    uint8_t  zero,flags;
    uint16_t base_hi;
} idt_entry_t;

typedef struct __attribute__((packed)){
    uint16_t limit; uint32_t base;
} idt_ptr_t;

static idt_entry_t   idt[256];
static idt_ptr_t     idtp;
static isr_handler_t handlers[256];

extern void idt_flush(uint32_t);
/* ISR stubs */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

static inline void outb(uint16_t p,uint8_t v){
    __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));
}
static inline uint8_t inb(uint16_t p){
    uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v;
}

static void gate(uint8_t n,uint32_t base,uint16_t sel,uint8_t fl){
    idt[n].base_lo=(uint16_t)(base&0xFFFF);
    idt[n].base_hi=(uint16_t)((base>>16)&0xFFFF);
    idt[n].sel=sel; idt[n].zero=0; idt[n].flags=fl;
}

static void pic_remap(void){
    uint8_t m=inb(0x21),s=inb(0xA1);
    outb(0x20,0x11); outb(0xA0,0x11);
    outb(0x21,0x20); outb(0xA1,0x28);
    outb(0x21,0x04); outb(0xA1,0x02);
    outb(0x21,0x01); outb(0xA1,0x01);
    outb(0x21,m);    outb(0xA1,s);
}

void isr_handler(registers_t *r){
    if(handlers[r->int_no]){ handlers[r->int_no](r); return; }
    vga_set_color(WHITE,RED);
    vga_puts("\n[ApexOS PANIC] Exception #");
    vga_print_dec(r->int_no);
    vga_puts(" at EIP="); vga_print_hex(r->eip);
    vga_puts("\nSystem halted.");
    __asm__ volatile("cli;hlt");
}

void irq_handler(registers_t *r){
    if(r->int_no>=40) outb(0xA0,0x20);
    outb(0x20,0x20);
    if(handlers[r->int_no]) handlers[r->int_no](r);
}

void idt_register_handler(uint8_t n, isr_handler_t h){ handlers[n]=h; }

void idt_init(void){
    pic_remap();
#define G(n) gate(n,(uint32_t)isr##n,0x08,0x8E)
    G(0);G(1);G(2);G(3);G(4);G(5);G(6);G(7);G(8);G(9);
    G(10);G(11);G(12);G(13);G(14);G(15);G(16);G(17);G(18);G(19);
    G(20);G(21);G(22);G(23);G(24);G(25);G(26);G(27);G(28);G(29);
    G(30);G(31);
#undef G
#define I(n,v) gate(v,(uint32_t)irq##n,0x08,0x8E)
    I(0,32);I(1,33);I(2,34);I(3,35);I(4,36);I(5,37);I(6,38);I(7,39);
    I(8,40);I(9,41);I(10,42);I(11,43);I(12,44);I(13,45);I(14,46);I(15,47);
#undef I
    idtp.limit=(uint16_t)(sizeof(idt)-1);
    idtp.base =(uint32_t)(uintptr_t)&idt;
    idt_flush((uint32_t)(uintptr_t)&idtp);
}
