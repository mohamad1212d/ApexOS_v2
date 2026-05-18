/* =====================================================
   drivers/keyboard.c - PS/2 Keyboard Driver
   Supports English + Arabic layout toggle (Alt)
===================================================== */
#include "apex.h"

#define KBD_DATA 0x60
#define BUF_SIZE 256

static char    buf[BUF_SIZE];
static uint8_t buf_head=0, buf_tail=0;
static bool    shift=false, ctrl=false, arabic=false;

static inline uint8_t inb(uint16_t p){
    uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v;
}

/* English scancode map */
static const char en_map[] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' '
};

static const char en_shift[] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,
    '*',0,' '
};

/* Arabic layout (basic - maps keys to Arabic chars using CP437 approximation) */
/* We store ASCII representations since VGA text mode uses CP437 */
static const char ar_map[] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t',
    /* q=ض w=ص e=ث r=ق t=ف y=غ u=ع i=ه o=خ p=ح */
    'D','S','T','Q','F','G','E','H','X','H','[',']','\n',
    0,
    /* a=ش s=س d=ي f=ب g=ل h=ا j=ت k=ن l=م */
    'C','R','Y','B','L','A','J','N','M',';','\'','`',
    0,'\\',
    /* z=ئ x=ء c=ؤ v=ر b=لا n=ى m=ة */
    'Z','\'','O','V','/','~','P',',','.','/',0,'*',0,' '
};

static void buf_push(char c){
    uint8_t next=(buf_tail+1)%BUF_SIZE;
    if(next!=buf_head){ buf[buf_tail]=c; buf_tail=next; }
}

static void kbd_cb(registers_t *r){
    (void)r;
    uint8_t sc=inb(KBD_DATA);

    /* Key release */
    if(sc&0x80){
        uint8_t rel=sc&0x7F;
        if(rel==0x2A||rel==0x36) shift=false;
        if(rel==0x1D) ctrl=false;
        return;
    }

    /* Special keys */
    if(sc==0x2A||sc==0x36){ shift=true; return; }
    if(sc==0x1D){ ctrl=true; return; }
    if(sc==0x38){ arabic=!arabic; return; } /* Alt = toggle Arabic */
    if(sc==0x3B){ /* F1 - show help */ buf_push('\x01'); return; }

    if(sc>= sizeof(en_map)) return;

    char c;
    if(arabic)
        c = ar_map[sc];
    else
        c = shift ? en_shift[sc] : en_map[sc];

    if(c) buf_push(c);
}

void keyboard_init(void){
    idt_register_handler(33, kbd_cb);
}

bool keyboard_haskey(void){ return buf_head!=buf_tail; }

char keyboard_getchar(void){
    while(!keyboard_haskey())
        __asm__ volatile("hlt");
    char c=buf[buf_head];
    buf_head=(buf_head+1)%BUF_SIZE;
    return c;
}
