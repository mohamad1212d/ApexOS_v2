/* =====================================================
   drivers/timer.c - PIT Timer (IRQ0)
===================================================== */
#include "apex.h"

static volatile uint32_t ticks = 0;
static uint32_t freq_hz = 0;

static inline void outb(uint16_t p,uint8_t v){
    __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));
}

static void timer_cb(registers_t *r){
    (void)r;
    ticks++;
}

void timer_init(uint32_t freq){
    freq_hz = freq;
    idt_register_handler(32, timer_cb);
    uint32_t div = 1193180 / freq;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(div & 0xFF));
    outb(0x40, (uint8_t)((div >> 8) & 0xFF));
}

uint32_t timer_get_ticks(void){ return ticks; }

void timer_sleep(uint32_t ms){
    uint32_t target = ticks + (ms * freq_hz / 1000);
    while(ticks < target)
        __asm__ volatile("hlt");
}
