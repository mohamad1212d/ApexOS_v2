#ifndef APEX_H
#define APEX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ══════════════════════════════════════════════
   VGA Driver
══════════════════════════════════════════════ */
typedef enum {
    BLACK=0, BLUE=1, GREEN=2, CYAN=3,
    RED=4, MAGENTA=5, BROWN=6, LIGHT_GREY=7,
    DARK_GREY=8, LIGHT_BLUE=9, LIGHT_GREEN=10,
    LIGHT_CYAN=11, LIGHT_RED=12, LIGHT_MAGENTA=13,
    YELLOW=14, WHITE=15
} vga_color_t;

void vga_init(void);
void vga_clear(void);
void vga_set_color(vga_color_t fg, vga_color_t bg);
void vga_putchar(char c);
void vga_puts(const char *s);
void vga_puts_at(size_t row, size_t col, const char *s);
void vga_set_cursor(size_t row, size_t col);
void vga_print_hex(uint32_t v);
void vga_print_dec(uint32_t v);
void vga_draw_hline(size_t row, size_t col, size_t len, char ch);
void vga_fill_rect(size_t row, size_t col, size_t h, size_t w,
                   char ch, vga_color_t fg, vga_color_t bg);

/* ══════════════════════════════════════════════
   GDT / IDT
══════════════════════════════════════════════ */
void gdt_init(void);
void idt_init(void);

typedef struct {
    uint32_t ds;
    uint32_t edi,esi,ebp,esp_dummy,ebx,edx,ecx,eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) registers_t;

typedef void (*isr_handler_t)(registers_t *);
void idt_register_handler(uint8_t n, isr_handler_t h);

/* ══════════════════════════════════════════════
   Timer / Keyboard
══════════════════════════════════════════════ */
void timer_init(uint32_t freq);
uint32_t timer_get_ticks(void);
void timer_sleep(uint32_t ms);

void keyboard_init(void);
char keyboard_getchar(void);
bool keyboard_haskey(void);

/* ══════════════════════════════════════════════
   Memory
══════════════════════════════════════════════ */
void  pmm_init(uint32_t mem_kb);
void *kmalloc(size_t sz);
void  kfree(void *ptr);
void *kmemset(void *dst, int c, size_t n);
void *kmemcpy(void *dst, const void *src, size_t n);
int   kstrcmp(const char *a, const char *b);
int   kstrncmp(const char *a, const char *b, size_t n);
size_t kstrlen(const char *s);
char *kstrcpy(char *dst, const char *src);
char *kstrcat(char *dst, const char *src);

/* ══════════════════════════════════════════════
   Shell
══════════════════════════════════════════════ */
void shell_run(void);

/* ══════════════════════════════════════════════
   Boot Animation
══════════════════════════════════════════════ */
void boot_animation(void);

#endif
