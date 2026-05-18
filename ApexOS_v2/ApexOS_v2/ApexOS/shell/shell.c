/* =====================================================
   shell/shell.c - ApexOS v2 Command Shell
   New: cd, pwd, rm, cp, mv, tree, stat,
        ifconfig, ping, arp, net
===================================================== */
#include "apex.h"
#include "fs.h"
#include "net.h"

static char     cwd[FS_MAX_PATH] = "/";
static bool     lang_ar          = false;
static uint32_t boot_ticks       = 0;

/* ── u32 to decimal string ───────────────────────── */
static void u32toa(uint32_t v, char *b) {
    if (!v) { b[0]='0'; b[1]=0; return; }
    char tmp[12]; int i=0;
    while (v) { tmp[i++]=(char)('0'+v%10); v/=10; }
    for (int j=0;j<i;j++) b[j]=tmp[i-1-j];
    b[i]=0;
}

static void println(const char *s) { vga_puts(s); vga_putchar('\n'); }

static void prompt(void) {
    vga_set_color(LIGHT_GREEN,BLACK); vga_puts("apex");
    vga_set_color(DARK_GREY,BLACK);   vga_puts("@");
    vga_set_color(LIGHT_CYAN,BLACK);  vga_puts("system");
    vga_set_color(WHITE,BLACK);       vga_puts(":");
    vga_set_color(YELLOW,BLACK);      vga_puts(cwd);
    vga_set_color(WHITE,BLACK);       vga_puts(" $ ");
    vga_set_color(LIGHT_GREY,BLACK);
}

static void draw_topbar(void) {
    vga_fill_rect(0,0,1,80,' ',BLACK,LIGHT_CYAN);
    vga_set_color(BLACK,LIGHT_CYAN);
    vga_puts_at(0,2,"ApexOS v2.0");
    vga_puts_at(0,27,"[ Shell + FS + Network ]");
    vga_puts_at(0,65,lang_ar?"AR":"EN");
    vga_puts_at(0,70,"Alt=Lang");
    vga_set_color(LIGHT_GREY,BLACK);
    vga_set_cursor(2,0);
}

static size_t readline(char *buf, size_t max) {
    size_t i=0;
    while (i<max-1) {
        char c=keyboard_getchar();
        if (c=='\n'||c=='\r'){vga_putchar('\n');break;}
        if (c=='\b'){if(i>0){i--;vga_putchar('\b');}continue;}
        if (c=='\x01'){kstrcpy(buf,"help");vga_puts("help\n");return 4;}
        buf[i++]=c; vga_putchar(c);
    }
    buf[i]=0; return i;
}

static char *skip_sp(char *s){while(*s==' ')s++;return s;}

static void resolve(const char *arg, char *out){
    if(arg[0]=='/') kstrcpy(out,arg);
    else fs_resolve(cwd,arg,out);
}

/* ─────── FILESYSTEM COMMANDS ─────────────────── */

static void cmd_ls(const char *arg){
    char path[FS_MAX_PATH]; resolve(arg[0]?arg:cwd,path);
    int ch[FS_MAX_FILES]; int n=fs_list(path,ch,FS_MAX_FILES);
    if(n<0){vga_set_color(LIGHT_RED,BLACK);println("ls: not found.");vga_set_color(LIGHT_GREY,BLACK);return;}
    vga_set_color(LIGHT_CYAN,BLACK);vga_puts("Contents of ");vga_puts(path);vga_putchar('\n');
    if(n==0){vga_set_color(DARK_GREY,BLACK);println("  (empty)");}
    for(int i=0;i<n;i++){
        fs_inode_t *nd=fs_get(ch[i]); if(!nd)continue;
        if(nd->type==FS_TYPE_DIR){vga_set_color(LIGHT_BLUE,BLACK);vga_puts("  [DIR]  ");vga_set_color(LIGHT_CYAN,BLACK);}
        else{vga_set_color(DARK_GREY,BLACK);vga_puts("  [FILE] ");vga_set_color(WHITE,BLACK);}
        vga_puts(nd->name);
        if(nd->type==FS_TYPE_FILE){char b[16];vga_set_color(DARK_GREY,BLACK);vga_puts("  (");u32toa(nd->size,b);vga_puts(b);vga_puts(" B)");}
        vga_putchar('\n');
    }
    vga_set_color(LIGHT_GREY,BLACK);
}

static void cmd_cd(const char *arg){
    char path[FS_MAX_PATH];
    if(!arg||!arg[0]){kstrcpy(cwd,"/");return;}
    resolve(arg,path);
    int idx=fs_find_path(path);
    if(idx<0){vga_set_color(LIGHT_RED,BLACK);println("cd: not found.");vga_set_color(LIGHT_GREY,BLACK);return;}
    fs_inode_t *n=fs_get(idx);
    if(!n||n->type!=FS_TYPE_DIR){vga_set_color(LIGHT_RED,BLACK);println("cd: not a directory.");vga_set_color(LIGHT_GREY,BLACK);return;}
    kstrcpy(cwd,path);
    if(!cwd[0])kstrcpy(cwd,"/");
}

static void cmd_mkdir(const char *arg){
    if(!arg||!arg[0]){println("Usage: mkdir <dir>");return;}
    char path[FS_MAX_PATH];resolve(arg,path);
    if(fs_create(path,FS_TYPE_DIR)<0){vga_set_color(YELLOW,BLACK);println("mkdir: exists or parent missing.");vga_set_color(LIGHT_GREY,BLACK);return;}
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("Created: ");println(path);vga_set_color(LIGHT_GREY,BLACK);
}

static void cmd_touch(const char *arg){
    if(!arg||!arg[0]){println("Usage: touch <file>");return;}
    char path[FS_MAX_PATH];resolve(arg,path);
    if(fs_create(path,FS_TYPE_FILE)<0){vga_set_color(YELLOW,BLACK);println("touch: exists or parent missing.");vga_set_color(LIGHT_GREY,BLACK);return;}
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("Created: ");println(path);vga_set_color(LIGHT_GREY,BLACK);
}

static void cmd_cat(const char *arg){
    if(!arg||!arg[0]){println("Usage: cat <file>");return;}
    char path[FS_MAX_PATH];resolve(arg,path);
    const char *content=fs_read(path);
    if(!content){vga_set_color(LIGHT_RED,BLACK);println("cat: not found or is directory.");vga_set_color(LIGHT_GREY,BLACK);return;}
    vga_set_color(DARK_GREY,BLACK);println("─────────────────────────────────");
    vga_set_color(WHITE,BLACK);println(content);
    vga_set_color(DARK_GREY,BLACK);println("─────────────────────────────────");
    vga_set_color(LIGHT_GREY,BLACK);
}

static void cmd_write(const char *arg){
    if(!arg||!arg[0]){println("Usage: write <file>");return;}
    char path[FS_MAX_PATH];resolve(arg,path);
    if(fs_find_path(path)<0)fs_create(path,FS_TYPE_FILE);
    fs_write(path,"");
    vga_set_color(YELLOW,BLACK);println("Type content (end with '.' on its own line):");
    char line[256];
    while(1){
        vga_set_color(CYAN,BLACK);vga_puts("> ");vga_set_color(WHITE,BLACK);
        readline(line,sizeof(line));
        if(kstrcmp(line,".")==0)break;
        fs_append(path,line);fs_append(path,"\n");
    }
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("Saved: ");println(path);vga_set_color(LIGHT_GREY,BLACK);
}

static void cmd_rm(const char *arg){
    if(!arg||!arg[0]){println("Usage: rm <path>");return;}
    char path[FS_MAX_PATH];resolve(arg,path);
    int r=fs_remove(path);
    if(r==-2){vga_set_color(YELLOW,BLACK);println("rm: dir not empty.");vga_set_color(LIGHT_GREY,BLACK);}
    else if(r<0){vga_set_color(LIGHT_RED,BLACK);println("rm: not found.");vga_set_color(LIGHT_GREY,BLACK);}
    else{vga_set_color(LIGHT_GREEN,BLACK);vga_puts("Removed: ");println(path);vga_set_color(LIGHT_GREY,BLACK);}
}

static void cmd_cp(const char *a,const char *b2){
    if(!a||!a[0]||!b2||!b2[0]){println("Usage: cp <src> <dst>");return;}
    char sp[FS_MAX_PATH],dp[FS_MAX_PATH];resolve(a,sp);resolve(b2,dp);
    if(fs_copy(sp,dp)<0){vga_set_color(LIGHT_RED,BLACK);println("cp: failed.");vga_set_color(LIGHT_GREY,BLACK);}
    else{vga_set_color(LIGHT_GREEN,BLACK);vga_puts("Copied ");vga_puts(sp);vga_puts(" -> ");println(dp);vga_set_color(LIGHT_GREY,BLACK);}
}

static void cmd_mv(const char *a,const char *b2){
    if(!a||!a[0]||!b2||!b2[0]){println("Usage: mv <src> <dst>");return;}
    char sp[FS_MAX_PATH],dp[FS_MAX_PATH];resolve(a,sp);resolve(b2,dp);
    if(fs_move(sp,dp)<0){vga_set_color(LIGHT_RED,BLACK);println("mv: failed.");vga_set_color(LIGHT_GREY,BLACK);}
    else{vga_set_color(LIGHT_GREEN,BLACK);vga_puts("Moved ");vga_puts(sp);vga_puts(" -> ");println(dp);vga_set_color(LIGHT_GREY,BLACK);}
}

static void cmd_stat(const char *arg){
    if(!arg||!arg[0]){println("Usage: stat <path>");return;}
    char path[FS_MAX_PATH];resolve(arg,path);
    int idx=fs_find_path(path);
    if(idx<0){vga_set_color(LIGHT_RED,BLACK);println("stat: not found.");vga_set_color(LIGHT_GREY,BLACK);return;}
    fs_inode_t *n=fs_get(idx);
    char b[16];
    vga_set_color(LIGHT_CYAN,BLACK);vga_puts("  Path: ");vga_set_color(WHITE,BLACK);println(path);
    vga_set_color(LIGHT_CYAN,BLACK);vga_puts("  Type: ");vga_set_color(WHITE,BLACK);println(n->type==FS_TYPE_DIR?"Directory":"File");
    vga_set_color(LIGHT_CYAN,BLACK);vga_puts("  Size: ");vga_set_color(WHITE,BLACK);u32toa(n->size,b);vga_puts(b);println(" bytes");
    vga_set_color(LIGHT_CYAN,BLACK);vga_puts("  Perm: ");vga_set_color(WHITE,BLACK);
    vga_puts((n->perms&FS_PERM_READ)?"r":"-");
    vga_puts((n->perms&FS_PERM_WRITE)?"w":"-");
    vga_puts((n->perms&FS_PERM_EXEC)?"x":"-");
    vga_putchar('\n');
    vga_set_color(LIGHT_GREY,BLACK);
}

static void tree_r(const char *path,int depth){
    int ch[FS_MAX_FILES];int n=fs_list(path,ch,FS_MAX_FILES);
    if(n<=0)return;
    for(int i=0;i<n;i++){
        fs_inode_t *nd=fs_get(ch[i]);if(!nd)continue;
        for(int d=0;d<depth;d++)vga_puts("  ");
        if(nd->type==FS_TYPE_DIR){
            vga_set_color(LIGHT_BLUE,BLACK);vga_puts("|__ ");vga_set_color(LIGHT_CYAN,BLACK);println(nd->name);
            char cp[FS_MAX_PATH];kstrcpy(cp,path);
            if(cp[kstrlen(cp)-1]!='/')kstrcat(cp,"/");
            kstrcat(cp,nd->name);tree_r(cp,depth+1);
        }else{
            vga_set_color(DARK_GREY,BLACK);vga_puts("|__ ");vga_set_color(WHITE,BLACK);println(nd->name);
        }
    }
    vga_set_color(LIGHT_GREY,BLACK);
}

static void cmd_tree(const char *arg){
    char path[FS_MAX_PATH];resolve(arg[0]?arg:cwd,path);
    vga_set_color(YELLOW,BLACK);println(path);
    tree_r(path,1);
    vga_set_color(LIGHT_GREY,BLACK);
}

/* ─────── NETWORK COMMANDS ───────────────────────── */

static void cmd_ifconfig(void){net_print_info();}

static void cmd_net(void){
    net_config_t *c=net_get_config();
    char b[24];
    vga_set_color(YELLOW,BLACK);
    println("╔══════════════════════════════════════════╗");
    println("║        Network Configuration             ║");
    println("╠══════════════════════════════════════════╣");
    vga_set_color(LIGHT_GREY,BLACK);
    ip_to_str(c->ip,b);      vga_puts("  IP:      ");vga_set_color(WHITE,BLACK);println(b);vga_set_color(LIGHT_GREY,BLACK);
    ip_to_str(c->gateway,b); vga_puts("  Gateway: ");vga_set_color(WHITE,BLACK);println(b);vga_set_color(LIGHT_GREY,BLACK);
    ip_to_str(c->netmask,b); vga_puts("  Netmask: ");vga_set_color(WHITE,BLACK);println(b);vga_set_color(LIGHT_GREY,BLACK);
    ip_to_str(c->dns,b);     vga_puts("  DNS:     ");vga_set_color(WHITE,BLACK);println(b);vga_set_color(LIGHT_GREY,BLACK);
    mac_to_str(c->mac,b);    vga_puts("  MAC:     ");vga_set_color(WHITE,BLACK);println(b);vga_set_color(LIGHT_GREY,BLACK);
    vga_puts("  Link:    ");
    vga_set_color(c->link_up?LIGHT_GREEN:LIGHT_RED,BLACK);
    println(c->link_up?"UP":"DOWN (simulated)");
    vga_set_color(YELLOW,BLACK);println("╚══════════════════════════════════════════╝");
    vga_set_color(LIGHT_GREY,BLACK);
}

static void cmd_ping(const char *arg){
    if(!arg||!arg[0]){println("Usage: ping <ip>");return;}
    ip_addr_t dest=ip_from_str(arg);
    char ip_s[20];ip_to_str(dest,ip_s);
    vga_set_color(LIGHT_GREY,BLACK);vga_puts("Pinging ");vga_puts(ip_s);println("...");
    ping_result_t r=net_ping(dest,64);
    if(r.reached){
        char ms[16];u32toa(r.rtt_ms,ms);
        vga_set_color(LIGHT_GREEN,BLACK);
        vga_puts("  Reply from ");vga_puts(ip_s);vga_puts(" time=");vga_puts(ms);println("ms TTL=64");
    }else{
        vga_set_color(LIGHT_RED,BLACK);println("  Request timed out (no link or no reply).");
    }
    vga_set_color(LIGHT_GREY,BLACK);
}

static void cmd_arp(void){
    net_config_t *c=net_get_config();
    char ip_s[20],mac_s[24];
    vga_set_color(LIGHT_CYAN,BLACK);
    println("  IP Address        MAC Address");
    println("  ─────────────────────────────────────");
    ip_to_str(c->ip,ip_s); mac_to_str(c->mac,mac_s);
    vga_set_color(LIGHT_GREY,BLACK);
    vga_puts("  ");vga_puts(ip_s);vga_puts("\t");println(mac_s);
}

/* ─────── SYSTEM COMMANDS ─────────────────────── */

static void cmd_help(void){
    vga_set_color(YELLOW,BLACK);
    println("╔═══════════════════════════════════════════════════╗");
    println("║         ApexOS v2.0 — Command Reference           ║");
    println("╠═══════════════════════════════════════════════════╣");
    vga_set_color(LIGHT_CYAN,BLACK);println("  FILESYSTEM COMMANDS");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  ls [path]      ");vga_set_color(LIGHT_GREY,BLACK);println("List directory");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  cd <path>      ");vga_set_color(LIGHT_GREY,BLACK);println("Change directory");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  pwd            ");vga_set_color(LIGHT_GREY,BLACK);println("Print working directory");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  mkdir <dir>    ");vga_set_color(LIGHT_GREY,BLACK);println("Create directory");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  touch <file>   ");vga_set_color(LIGHT_GREY,BLACK);println("Create file");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  cat <file>     ");vga_set_color(LIGHT_GREY,BLACK);println("Show file contents");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  write <file>   ");vga_set_color(LIGHT_GREY,BLACK);println("Write to file");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  rm <path>      ");vga_set_color(LIGHT_GREY,BLACK);println("Remove file/dir");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  cp <src> <dst> ");vga_set_color(LIGHT_GREY,BLACK);println("Copy file");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  mv <src> <dst> ");vga_set_color(LIGHT_GREY,BLACK);println("Move/rename file");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  stat <path>    ");vga_set_color(LIGHT_GREY,BLACK);println("File info");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  tree [path]    ");vga_set_color(LIGHT_GREY,BLACK);println("Directory tree");
    vga_set_color(LIGHT_CYAN,BLACK);println("  NETWORK COMMANDS");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  ifconfig       ");vga_set_color(LIGHT_GREY,BLACK);println("Network interface info");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  net            ");vga_set_color(LIGHT_GREY,BLACK);println("Network configuration");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  ping <ip>      ");vga_set_color(LIGHT_GREY,BLACK);println("Ping an IP");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  arp            ");vga_set_color(LIGHT_GREY,BLACK);println("ARP table");
    vga_set_color(LIGHT_CYAN,BLACK);println("  SYSTEM COMMANDS");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  sysinfo        ");vga_set_color(LIGHT_GREY,BLACK);println("System info");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  uptime         ");vga_set_color(LIGHT_GREY,BLACK);println("Uptime");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  echo <text>    ");vga_set_color(LIGHT_GREY,BLACK);println("Print text");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  lang           ");vga_set_color(LIGHT_GREY,BLACK);println("Toggle AR/EN");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  clear          ");vga_set_color(LIGHT_GREY,BLACK);println("Clear screen");
    vga_set_color(LIGHT_GREEN,BLACK);vga_puts("  reboot         ");vga_set_color(LIGHT_GREY,BLACK);println("Reboot");
    vga_set_color(YELLOW,BLACK);
    println("╚═══════════════════════════════════════════════════╝");
    vga_set_color(DARK_GREY,BLACK);println("  Tip: Alt=toggle language | F1=help | cd ..=go up");
    vga_set_color(LIGHT_GREY,BLACK);
}

static void cmd_sysinfo(void){
    char b[16];
    vga_set_color(LIGHT_CYAN,BLACK);
    println("╔══════════════════════════════════════╗");
    println("║         ApexOS v2.0 Sysinfo          ║");
    println("╠══════════════════════════════════════╣");
    vga_set_color(LIGHT_GREY,BLACK);
    vga_puts("  OS:      ");vga_set_color(WHITE,BLACK);println("ApexOS v2.0");vga_set_color(LIGHT_GREY,BLACK);
    vga_puts("  Arch:    ");vga_set_color(WHITE,BLACK);println("x86 32-bit");vga_set_color(LIGHT_GREY,BLACK);
    vga_puts("  FS:      ");vga_set_color(WHITE,BLACK);println("ApexFS (hierarchical)");vga_set_color(LIGHT_GREY,BLACK);
    vga_puts("  Files:   ");vga_set_color(WHITE,BLACK);
    u32toa(fs_total_files(),b);vga_puts(b);vga_puts(" files / ");
    u32toa(fs_total_dirs(),b);vga_puts(b);println(" dirs");vga_set_color(LIGHT_GREY,BLACK);
    vga_puts("  Network: ");vga_set_color(WHITE,BLACK);
    println(net_get_config()->link_up?"RTL8139 LINK UP":"Simulated / No HW");vga_set_color(LIGHT_GREY,BLACK);
    vga_puts("  Lang:    ");vga_set_color(WHITE,BLACK);println(lang_ar?"Arabic":"English");
    vga_set_color(LIGHT_CYAN,BLACK);println("╚══════════════════════════════════════╝");
    vga_set_color(LIGHT_GREY,BLACK);
}

static void cmd_uptime(void){
    uint32_t s=(timer_get_ticks()-boot_ticks)/100;
    uint32_t m=s/60;s%=60;uint32_t h=m/60;m%=60;
    char b[8];
    vga_set_color(LIGHT_GREY,BLACK);vga_puts("Uptime: ");vga_set_color(WHITE,BLACK);
    u32toa(h,b);vga_puts(b);vga_puts("h ");
    u32toa(m,b);vga_puts(b);vga_puts("m ");
    u32toa(s,b);vga_puts(b);println("s");
    vga_set_color(LIGHT_GREY,BLACK);
}

/* ── Main shell loop ─────────────────────────────── */
void shell_run(void){
    fs_init();
    net_init();
    boot_ticks=timer_get_ticks();

    vga_clear();draw_topbar();vga_set_cursor(2,0);

    vga_set_color(LIGHT_CYAN,BLACK);
    println("  ___                 ___  ___ ");
    println(" / _ \\ ___  ___ __ __/ _ \\/ __/");
    println("/ __ |/ _ \\/ -_)\\ \\ / // /\\ \\  ");
    println("/_/ |_/ .__/\\__//_/\\_/\\___/___/  v2.0");
    println("     /_/");
    vga_set_color(DARK_GREY,BLACK);
    println("─────────────────────────────────────────────────────");
    vga_set_color(LIGHT_GREY,BLACK);
    println("ApexOS v2.0 — Enhanced Filesystem + Network Stack");
    vga_set_color(LIGHT_GREEN,BLACK);
    println("NEW: cd pwd rm cp mv tree stat | ifconfig net ping arp");
    vga_set_color(DARK_GREY,BLACK);
    println("Type 'help' for all commands. Alt = toggle Arabic/EN.");
    println("─────────────────────────────────────────────────────");
    vga_set_color(LIGHT_GREY,BLACK);vga_putchar('\n');

    char line[256];
    while(1){
        prompt();
        readline(line,sizeof(line));
        char *cmd=skip_sp(line);
        if(!*cmd)continue;

        char *arg=cmd;while(*arg&&*arg!=' ')arg++;
        if(*arg){*arg++=0;arg=skip_sp(arg);}
        char *arg2=arg;while(*arg2&&*arg2!=' ')arg2++;
        if(*arg2){*arg2++=0;arg2=skip_sp(arg2);}

        if     (kstrcmp(cmd,"help")==0)     cmd_help();
        else if(kstrcmp(cmd,"ls")==0)       cmd_ls(arg);
        else if(kstrcmp(cmd,"cd")==0)       cmd_cd(arg);
        else if(kstrcmp(cmd,"pwd")==0)      {vga_set_color(WHITE,BLACK);println(cwd);vga_set_color(LIGHT_GREY,BLACK);}
        else if(kstrcmp(cmd,"mkdir")==0)    cmd_mkdir(arg);
        else if(kstrcmp(cmd,"touch")==0)    cmd_touch(arg);
        else if(kstrcmp(cmd,"cat")==0)      cmd_cat(arg);
        else if(kstrcmp(cmd,"write")==0)    cmd_write(arg);
        else if(kstrcmp(cmd,"rm")==0)       cmd_rm(arg);
        else if(kstrcmp(cmd,"cp")==0)       cmd_cp(arg,arg2);
        else if(kstrcmp(cmd,"mv")==0)       cmd_mv(arg,arg2);
        else if(kstrcmp(cmd,"stat")==0)     cmd_stat(arg);
        else if(kstrcmp(cmd,"tree")==0)     cmd_tree(arg);
        else if(kstrcmp(cmd,"ifconfig")==0) cmd_ifconfig();
        else if(kstrcmp(cmd,"net")==0)      cmd_net();
        else if(kstrcmp(cmd,"ping")==0)     cmd_ping(arg);
        else if(kstrcmp(cmd,"arp")==0)      cmd_arp();
        else if(kstrcmp(cmd,"echo")==0)     {vga_set_color(WHITE,BLACK);println(arg);vga_set_color(LIGHT_GREY,BLACK);}
        else if(kstrcmp(cmd,"sysinfo")==0)  cmd_sysinfo();
        else if(kstrcmp(cmd,"uptime")==0)   cmd_uptime();
        else if(kstrcmp(cmd,"lang")==0)     {lang_ar=!lang_ar;draw_topbar();vga_set_cursor(2,0);}
        else if(kstrcmp(cmd,"clear")==0)    {vga_clear();draw_topbar();vga_set_cursor(2,0);}
        else if(kstrcmp(cmd,"reboot")==0)   {vga_set_color(YELLOW,BLACK);println("Rebooting...");timer_sleep(1000);__asm__ volatile("int $0xFF");}
        else{
            vga_set_color(LIGHT_RED,BLACK);vga_puts("Unknown: ");
            vga_set_color(WHITE,BLACK);vga_puts(cmd);
            vga_set_color(DARK_GREY,BLACK);println("  (type 'help')");
            vga_set_color(LIGHT_GREY,BLACK);
        }
    }
}
