/* =====================================================
   kernel/kernel.c - ApexOS Main Kernel
===================================================== */
#include "apex.h"
#include "fs.h"
#include "net.h"

/* Multiboot info structure (partial) */
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    /* ... more fields ... */
} __attribute__((packed)) multiboot_info_t;

void kernel_main(uint32_t magic, multiboot_info_t *mbi){
    /* 1. Initialize VGA first so we can print errors */
    vga_init();

    /* 2. Setup GDT (segmentation) */
    gdt_init();

    /* 3. Setup IDT (interrupt handling) */
    idt_init();

    /* 4. Initialize memory */
    uint32_t mem_kb = 640; /* default safe value */
    if(magic == 0x2BADB002 && mbi){
        mem_kb = mbi->mem_lower + mbi->mem_upper;
    }
    pmm_init(mem_kb);

    /* 5. Start PIT timer at 100 Hz */
    timer_init(100);

    /* 6. Initialize keyboard */
    keyboard_init();

    /* 7. Enable interrupts */
    __asm__ volatile("sti");

    /* 8. Boot animation (Windows-style) */
    boot_animation();

    /* 9. Run the shell */
    shell_run();

    /* Should never reach here */
    __asm__ volatile("cli; hlt");
}
