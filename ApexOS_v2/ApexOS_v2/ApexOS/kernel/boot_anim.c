/* =====================================================
   kernel/boot_anim.c - ApexOS Boot Animation
   Windows-style loading screen with spinner
===================================================== */
#include "apex.h"

/* Draw the ApexOS logo */
static void draw_logo(void){
    vga_set_color(LIGHT_CYAN, BLACK);
    vga_puts_at(6, 28, "    ___    ____  _______  __   ____  _____");
    vga_puts_at(7, 28, "   /   |  / __ \\/ ____/ |/ /  / __ \\/ ___/");
    vga_puts_at(8, 28, "  / /| | / /_/ / __/  |   /  / / / /\\__ \\ ");
    vga_puts_at(9, 28, " / ___ |/ ____/ /___ /   |  / /_/ /___/ / ");
    vga_puts_at(10,28, "/_/  |_/_/   /_____//_/|_|  \\____//____/  ");
}

/* Windows-style dot spinner */
static void draw_spinner(int frame){
    /* 4 dots that animate like Windows boot */
    size_t base_col = 32;
    size_t row_pos  = 16;

    for(int i = 0; i < 4; i++){
        int active = (frame % 8);
        bool lit   = (i == active || i == (active-1+4)%4);

        if(lit)
            vga_set_color(WHITE, BLACK);
        else
            vga_set_color(DARK_GREY, BLACK);

        /* Draw filled circle using block char */
        vga_puts_at(row_pos, base_col + i*4, "\x04"); /* ♦ dot */
    }
}

/* Progress bar */
static void draw_progress(int pct){
    size_t bar_row = 19;
    size_t bar_col = 20;
    size_t bar_w   = 40;
    size_t filled  = (size_t)(pct * (int)bar_w / 100);

    vga_set_color(DARK_GREY, BLACK);
    vga_puts_at(bar_row, bar_col - 2, "[");
    vga_puts_at(bar_row, bar_col + bar_w, "]");

    for(size_t i = 0; i < bar_w; i++){
        if(i < filled)
            vga_set_color(LIGHT_CYAN, DARK_GREY);
        else
            vga_set_color(DARK_GREY, BLACK);
        vga_set_cursor(bar_row, bar_col + i);
        vga_putchar(i < filled ? '\xDB' : '\xB0'); /* block chars */
    }
}

/* Status messages shown during boot */
static const char *boot_msgs[] = {
    "Initializing hardware...",
    "Loading kernel modules...",
    "Setting up memory...",
    "Starting services...",
    "Preparing desktop...",
    "ApexOS is ready!"
};

void boot_animation(void){
    vga_clear();

    /* Black background */
    vga_fill_rect(0, 0, 25, 80, ' ', BLACK, BLACK);

    /* Draw logo */
    draw_logo();

    /* Version info */
    vga_set_color(DARK_GREY, BLACK);
    vga_puts_at(12, 33, "ApexOS v1.0 - Built with love");

    /* Divider */
    vga_set_color(DARK_GREY, BLACK);
    for(int i=20; i<60; i++){
        vga_set_cursor(14, (size_t)i);
        vga_putchar('\xC4'); /* ─ */
    }

    /* "Please wait" text */
    vga_set_color(LIGHT_GREY, BLACK);
    vga_puts_at(15, 30, "Please wait...");

    /* Arabic text - "جاري التحميل" phonetically */
    vga_set_color(YELLOW, BLACK);
    vga_puts_at(20, 30, "| Jari al-Tahmel... |");

    /* Animate the spinner + progress bar */
    int progress = 0;
    int msg_idx  = 0;
    int frame    = 0;

    while(progress <= 100){
        /* Clear status line */
        vga_fill_rect(22, 0, 1, 80, ' ', DARK_GREY, BLACK);

        /* Show current boot message */
        if(msg_idx < 6){
            vga_set_color(LIGHT_GREY, BLACK);
            size_t msg_len = kstrlen(boot_msgs[msg_idx]);
            vga_puts_at(22, (80 - msg_len) / 2, boot_msgs[msg_idx]);
        }

        draw_spinner(frame);
        draw_progress(progress);

        /* Delay */
        timer_sleep(80);

        frame++;
        progress += 2;
        if(progress % 17 == 0 && msg_idx < 5) msg_idx++;
    }

    /* Final flash */
    vga_set_color(WHITE, BLACK);
    vga_fill_rect(22, 0, 1, 80, ' ', WHITE, BLACK);
    vga_puts_at(22, 33, "Welcome to ApexOS!");
    timer_sleep(800);

    vga_clear();
}
