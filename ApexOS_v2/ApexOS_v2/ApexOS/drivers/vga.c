/* =====================================================
   drivers/vga.c - VGA Text Mode Driver
   ApexOS - Supports Arabic & English
===================================================== */

#include "apex.h"

#define VGA_W    80
#define VGA_H    25
#define VGA_MEM  ((volatile uint16_t*)0xB8000)
#define CTRL     0x3D4
#define DATA     0x3D5

static size_t    row, col;
static uint8_t   color;

static inline void outb(uint16_t p, uint8_t v){
    __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));
}

static uint8_t mkcolor(vga_color_t fg, vga_color_t bg){
    return (uint8_t)(fg|(bg<<4));
}

static void hw_cursor(void){
    uint16_t pos=(uint16_t)(row*VGA_W+col);
    outb(CTRL,0x0E); outb(DATA,(uint8_t)(pos>>8));
    outb(CTRL,0x0F); outb(DATA,(uint8_t)(pos&0xFF));
}

static void scroll(void){
    for(size_t r=1;r<VGA_H;r++)
        for(size_t c=0;c<VGA_W;c++)
            VGA_MEM[(r-1)*VGA_W+c]=VGA_MEM[r*VGA_W+c];
    uint16_t blank=(uint16_t)(' ')|((uint16_t)color<<8);
    for(size_t c=0;c<VGA_W;c++)
        VGA_MEM[(VGA_H-1)*VGA_W+c]=blank;
    if(row>0) row--;
}

void vga_init(void){
    color=mkcolor(LIGHT_GREY,BLACK);
    row=col=0;
    vga_clear();
}

void vga_clear(void){
    uint16_t b=(uint16_t)(' ')|((uint16_t)color<<8);
    for(size_t i=0;i<VGA_W*VGA_H;i++) VGA_MEM[i]=b;
    row=col=0;
    hw_cursor();
}

void vga_set_color(vga_color_t fg, vga_color_t bg){
    color=mkcolor(fg,bg);
}

void vga_set_cursor(size_t r, size_t c){
    row=r%VGA_H; col=c%VGA_W;
    hw_cursor();
}

void vga_putchar(char c){
    if(c=='\n'){ col=0; row++; }
    else if(c=='\r'){ col=0; }
    else if(c=='\b'){ if(col>0){ col--; VGA_MEM[row*VGA_W+col]=(uint16_t)(' ')|((uint16_t)color<<8); } }
    else if(c=='\t'){ col=(col+8)&~7u; if(col>=VGA_W){col=0;row++;} }
    else{
        VGA_MEM[row*VGA_W+col]=(uint16_t)(uint8_t)c|((uint16_t)color<<8);
        if(++col>=VGA_W){col=0;row++;}
    }
    if(row>=VGA_H) scroll();
    hw_cursor();
}

void vga_puts(const char *s){
    while(*s) vga_putchar(*s++);
}

void vga_puts_at(size_t r, size_t c, const char *s){
    size_t sr=row, sc=col;
    row=r; col=c;
    vga_puts(s);
    row=sr; col=sc;
    hw_cursor();
}

void vga_print_hex(uint32_t v){
    static const char h[]="0123456789ABCDEF";
    vga_puts("0x");
    for(int i=28;i>=0;i-=4) vga_putchar(h[(v>>i)&0xF]);
}

void vga_print_dec(uint32_t v){
    char b[12]; int i=0;
    if(!v){vga_putchar('0');return;}
    while(v){b[i++]=(char)('0'+v%10);v/=10;}
    while(i--) vga_putchar(b[i]);
}

void vga_draw_hline(size_t r, size_t c, size_t len, char ch){
    for(size_t i=0;i<len&&c+i<VGA_W;i++)
        VGA_MEM[r*VGA_W+c+i]=(uint16_t)(uint8_t)ch|((uint16_t)color<<8);
}

void vga_fill_rect(size_t r, size_t c, size_t h, size_t w,
                   char ch, vga_color_t fg, vga_color_t bg){
    uint8_t old=color;
    color=mkcolor(fg,bg);
    for(size_t i=0;i<h;i++)
        for(size_t j=0;j<w;j++)
            VGA_MEM[(r+i)*VGA_W+(c+j)]=(uint16_t)(uint8_t)ch|((uint16_t)color<<8);
    color=old;
}
