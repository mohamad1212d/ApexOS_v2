/* =====================================================
   drivers/net.c - ApexOS Network Stack
   RTL8139 / NE2000 Ethernet + ARP + IP + ICMP + UDP

   On real hardware: scans PCI bus for RTL8139 (0x10EC:0x8139).
   On emulators (QEMU with -net nic,model=ne2k_pci or rtl8139):
     - RTL8139 is auto-detected.
   Simulator fallback: if no card found, the driver marks
     link_up=false and all sends return -1 gracefully.
===================================================== */
#include "apex.h"
#include "net.h"

/* ── PCI helpers (bare-metal, x86 port I/O) ─────── */
static inline void outb(uint16_t port, uint8_t  val){ __asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port)); }
static inline void outw(uint16_t port, uint16_t val){ __asm__ volatile("outw %0,%1"::"a"(val),"Nd"(port)); }
static inline void outl(uint16_t port, uint32_t val){ __asm__ volatile("outl %0,%1"::"a"(val),"Nd"(port)); }
static inline uint8_t  inb(uint16_t port){ uint8_t  v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port)); return v; }
static inline uint16_t inw(uint16_t port){ uint16_t v; __asm__ volatile("inw %1,%0":"=a"(v):"Nd"(port)); return v; }
static inline uint32_t inl(uint16_t port){ uint32_t v; __asm__ volatile("inl %1,%0":"=a"(v):"Nd"(port)); return v; }

/* ── PCI configuration space ─────────────────────── */
#define PCI_CFG_ADDR  0xCF8
#define PCI_CFG_DATA  0xCFC

static uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    uint32_t addr = (1u<<31) | ((uint32_t)bus<<16) |
                    ((uint32_t)dev<<11) | ((uint32_t)func<<8) |
                    (off & 0xFC);
    outl(PCI_CFG_ADDR, addr);
    return inl(PCI_CFG_DATA);
}

/* ── RTL8139 register offsets ───────────────────── */
#define RTL_IDR0        0x00   /* MAC address (6 bytes) */
#define RTL_MAR0        0x08   /* Multicast filter */
#define RTL_TXSTATUS0   0x10   /* TX status registers (4 × 32-bit) */
#define RTL_TXADDR0     0x20   /* TX buffer addresses (4 × 32-bit) */
#define RTL_RXBUF       0x30   /* RX buffer start address */
#define RTL_CMD         0x37   /* Command register */
#define RTL_RXBUF_PTR   0x38   /* RX buffer read pointer */
#define RTL_IMR         0x3C   /* Interrupt mask */
#define RTL_ISR         0x3E   /* Interrupt status */
#define RTL_TCR         0x40   /* TX config */
#define RTL_RCR         0x44   /* RX config */
#define RTL_CONFIG1     0x52

#define RTL_CMD_RESET   0x10
#define RTL_CMD_RX_EN   0x08
#define RTL_CMD_TX_EN   0x04

#define RTL_RCR_AAP     (1<<0)  /* accept all physical */
#define RTL_RCR_APM     (1<<1)  /* accept physical match */
#define RTL_RCR_AM      (1<<2)  /* accept multicast */
#define RTL_RCR_AB      (1<<3)  /* accept broadcast */
#define RTL_RCR_WRAP    (1<<7)  /* ring buffer wrap */
#define RTL_RCR_RBLEN   (0<<11) /* 8K RX buffer */

/* ── Driver state ────────────────────────────────── */
static net_config_t cfg;
static uint16_t     rtl_iobase = 0;

/* RX ring buffer (8KB + header overhead) */
#define RX_BUF_SIZE  (8192 + 16 + 1500)
static uint8_t  rx_ring[RX_BUF_SIZE];
static uint32_t rx_ptr  = 0;

/* TX buffers (4 × 1536 aligned) */
#define TX_BUF_SIZE  1536
static uint8_t tx_bufs[4][TX_BUF_SIZE] __attribute__((aligned(4)));
static int     tx_cur = 0;

/* ARP table */
static arp_entry_t arp_table[ARP_TABLE_SIZE];

/* Packet receive queue */
static net_packet_t rx_queue[NET_RX_BUFFERS];

/* ── Byte-swap helpers ───────────────────────────── */
static inline uint16_t bswap16(uint16_t x){ return (uint16_t)((x<<8)|(x>>8)); }
static inline uint32_t bswap32(uint32_t x){
    return ((x&0xFF)<<24)|((x&0xFF00)<<8)|((x>>8)&0xFF00)|((x>>24)&0xFF);
}
#define htons(x) bswap16(x)
#define ntohs(x) bswap16(x)
#define htonl(x) bswap32(x)
#define ntohl(x) bswap32(x)

/* ── Checksum ────────────────────────────────────── */
uint16_t ip_checksum(const void *data, uint16_t len) {
    const uint16_t *p = (const uint16_t *)data;
    uint32_t sum = 0;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(const uint8_t *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

/* ── PCI scan for RTL8139 ────────────────────────── */
static uint16_t pci_find_rtl8139(void) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            uint32_t id = pci_read((uint8_t)bus, dev, 0, 0);
            uint16_t vendor = id & 0xFFFF;
            uint16_t device = id >> 16;
            if (vendor == RTL8139_VENDOR && device == RTL8139_DEVICE) {
                /* BAR0: IO base */
                uint32_t bar0 = pci_read((uint8_t)bus, dev, 0, 0x10);
                if (bar0 & 1) return (uint16_t)(bar0 & 0xFFFC);
            }
        }
    }
    return 0;
}

/* ── RTL8139 initialisation ──────────────────────── */
static int rtl_init(uint16_t iobase) {
    /* Power on */
    outb(iobase + RTL_CONFIG1, 0x00);

    /* Software reset */
    outb(iobase + RTL_CMD, RTL_CMD_RESET);
    uint32_t timeout = 1000;
    while ((inb(iobase + RTL_CMD) & RTL_CMD_RESET) && --timeout);
    if (!timeout) return -1;

    /* Read MAC */
    for (int i = 0; i < 6; i++)
        cfg.mac[i] = inb(iobase + RTL_IDR0 + i);

    /* Setup RX buffer */
    outl(iobase + RTL_RXBUF, (uint32_t)(uintptr_t)rx_ring);

    /* Interrupts: RX OK + TX OK + TX ERR + RX ERR */
    outw(iobase + RTL_IMR, 0x0005);

    /* RX config: accept broadcast + physical + multicast, 8K buf, no wrap */
    outl(iobase + RTL_RCR,
         RTL_RCR_AB | RTL_RCR_APM | RTL_RCR_AM |
         RTL_RCR_AAP | RTL_RCR_RBLEN);

    /* TX config: IFG=3 (standard IEEE gap) */
    outl(iobase + RTL_TCR, 0x03000700);

    /* Enable TX + RX */
    outb(iobase + RTL_CMD, RTL_CMD_RX_EN | RTL_CMD_TX_EN);

    /* TX buffer addresses */
    for (int i = 0; i < 4; i++)
        outl(iobase + RTL_TXADDR0 + i * 4,
             (uint32_t)(uintptr_t)tx_bufs[i]);

    return 0;
}

/* ── net_init ─────────────────────────────────────── */
int net_init(void) {
    kmemset(&cfg, 0, sizeof(cfg));
    kmemset(arp_table, 0, sizeof(arp_table));
    kmemset(rx_queue, 0, sizeof(rx_queue));
    rx_ptr = 0;

    /* Default IP config (static) */
    cfg.ip      = ip_from_str("10.0.2.15");
    cfg.gateway = ip_from_str("10.0.2.2");
    cfg.netmask = ip_from_str("255.255.255.0");
    cfg.dns     = ip_from_str("8.8.8.8");

    /* Try to find RTL8139 */
    rtl_iobase = pci_find_rtl8139();
    if (rtl_iobase && rtl_init(rtl_iobase) == 0) {
        cfg.initialized = true;
        cfg.link_up     = true;
        return 0;
    }

    /* Fallback: simulate a MAC for display purposes */
    cfg.mac[0] = 0x52; cfg.mac[1] = 0x54;
    cfg.mac[2] = 0x00; cfg.mac[3] = 0xDE;
    cfg.mac[4] = 0xAD; cfg.mac[5] = 0x01;
    cfg.initialized = true;
    cfg.link_up     = false; /* no real hardware */
    return -1;
}

/* ── net_send ─────────────────────────────────────── */
int net_send(const uint8_t *frame, uint16_t len) {
    if (!cfg.link_up) return -1;
    if (len > TX_BUF_SIZE) return -1;

    kmemcpy(tx_bufs[tx_cur], frame, len);
    /* Pad to 64 bytes min */
    if (len < 60) {
        kmemset(tx_bufs[tx_cur] + len, 0, 60 - len);
        len = 60;
    }

    uint16_t base = rtl_iobase;
    /* Write length + OWN bit to start TX */
    outl(base + RTL_TXSTATUS0 + tx_cur * 4, (uint32_t)len);

    /* Wait for TX done (TOK bit = 15) */
    uint32_t timeout = 100000;
    while (!(inl(base + RTL_TXSTATUS0 + tx_cur * 4) & (1 << 15)) && --timeout);

    tx_cur = (tx_cur + 1) & 3;
    return (timeout > 0) ? 0 : -1;
}

/* ── net_poll – drain RX ring buffer ─────────────── */
int net_poll(void) {
    if (!cfg.link_up) return 0;
    int count = 0;

    while (!(inb(rtl_iobase + RTL_CMD) & 0x01)) { /* BUFE=0 means data */
        /* RTL8139 packet header (16-bit status + 16-bit length) */
        uint16_t hdr_off = rx_ptr;
        /* uint16_t pkt_status = *(uint16_t*)(rx_ring + hdr_off); */
        uint16_t pkt_len  = *(uint16_t*)(rx_ring + hdr_off + 2);

        if (pkt_len == 0 || pkt_len > ETH_FRAME_MAX) break;

        /* Find a free RX slot */
        for (int i = 0; i < NET_RX_BUFFERS; i++) {
            if (!rx_queue[i].used) {
                uint16_t copy_len = pkt_len - 4; /* strip CRC */
                if (copy_len > NET_BUF_SIZE) copy_len = NET_BUF_SIZE;
                kmemcpy(rx_queue[i].data, rx_ring + hdr_off + 4, copy_len);
                rx_queue[i].len  = copy_len;
                rx_queue[i].used = true;
                break;
            }
        }

        /* Advance RX pointer (4-byte aligned) */
        rx_ptr = (rx_ptr + pkt_len + 4 + 3) & ~3u;
        rx_ptr %= RX_BUF_SIZE;
        outw(rtl_iobase + RTL_RXBUF_PTR, (uint16_t)(rx_ptr - 16));
        count++;
    }
    return count;
}

/* ── IP helpers ──────────────────────────────────── */
ip_addr_t ip_from_str(const char *s) {
    uint32_t a=0,b=0,c=0,d=0;
    /* Simple parser */
    while (*s && *s!='.') { a=a*10+(*s-'0'); s++; } if (*s) s++;
    while (*s && *s!='.') { b=b*10+(*s-'0'); s++; } if (*s) s++;
    while (*s && *s!='.') { c=c*10+(*s-'0'); s++; } if (*s) s++;
    while (*s)            { d=d*10+(*s-'0'); s++; }
    return htonl((a<<24)|(b<<16)|(c<<8)|d);
}

void ip_to_str(ip_addr_t ip, char *out) {
    uint32_t h = ntohl(ip);
    /* simple decimal print for each octet */
    uint8_t oct[4] = { (uint8_t)(h>>24), (uint8_t)(h>>16),
                       (uint8_t)(h>>8),  (uint8_t)h };
    char *p = out;
    for (int i = 0; i < 4; i++) {
        uint8_t v = oct[i];
        if (v >= 100) { *p++ = (char)('0' + v/100); v %= 100; *p++ = (char)('0' + v/10); v %= 10; }
        else if (v >= 10) { *p++ = (char)('0' + v/10); v %= 10; }
        *p++ = (char)('0' + v);
        if (i < 3) *p++ = '.';
    }
    *p = 0;
}

void mac_to_str(const mac_addr_t mac, char *out) {
    static const char hex[] = "0123456789ABCDEF";
    char *p = out;
    for (int i = 0; i < 6; i++) {
        *p++ = hex[mac[i] >> 4];
        *p++ = hex[mac[i] & 0xF];
        if (i < 5) *p++ = ':';
    }
    *p = 0;
}

/* ── ARP ─────────────────────────────────────────── */

/* ARP packet structure */
typedef struct __attribute__((packed)) {
    uint8_t  dst_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;   /* 0x0806 */
    uint16_t htype;       /* 1 = Ethernet */
    uint16_t ptype;       /* 0x0800 = IPv4 */
    uint8_t  hlen;        /* 6 */
    uint8_t  plen;        /* 4 */
    uint16_t oper;        /* 1=req, 2=reply */
    uint8_t  sha[6];      /* sender MAC */
    uint32_t spa;         /* sender IP */
    uint8_t  tha[6];      /* target MAC */
    uint32_t tpa;         /* target IP */
} arp_pkt_t;

static void arp_cache(ip_addr_t ip, const mac_addr_t mac) {
    /* check if already cached */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && arp_table[i].ip == ip) {
            kmemcpy(arp_table[i].mac, mac, 6);
            return;
        }
    }
    /* find empty slot */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (!arp_table[i].valid) {
            arp_table[i].ip    = ip;
            arp_table[i].valid = true;
            kmemcpy(arp_table[i].mac, mac, 6);
            return;
        }
    }
    /* evict slot 0 */
    arp_table[0].ip    = ip;
    arp_table[0].valid = true;
    kmemcpy(arp_table[0].mac, mac, 6);
}

int arp_resolve(ip_addr_t ip, mac_addr_t out_mac) {
    /* Check cache */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && arp_table[i].ip == ip) {
            kmemcpy(out_mac, arp_table[i].mac, 6);
            return 0;
        }
    }

    if (!cfg.link_up) return -1;

    /* Send ARP request */
    arp_pkt_t pkt;
    kmemset(&pkt, 0, sizeof(pkt));

    /* Ethernet header */
    kmemset(pkt.dst_mac, 0xFF, 6);          /* broadcast */
    kmemcpy(pkt.src_mac, cfg.mac, 6);
    pkt.ethertype = htons(ETH_TYPE_ARP);

    /* ARP payload */
    pkt.htype = htons(1);
    pkt.ptype = htons(ETH_TYPE_IP);
    pkt.hlen  = 6;
    pkt.plen  = 4;
    pkt.oper  = htons(1); /* request */
    kmemcpy(pkt.sha, cfg.mac, 6);
    pkt.spa   = cfg.ip;
    kmemset(pkt.tha, 0, 6);
    pkt.tpa   = ip;

    net_send((uint8_t*)&pkt, sizeof(pkt));

    /* Wait for reply (poll RX up to ~200ms) */
    for (int t = 0; t < 200; t++) {
        timer_sleep(1);
        net_poll();
        for (int i = 0; i < NET_RX_BUFFERS; i++) {
            if (!rx_queue[i].used) continue;
            arp_pkt_t *r = (arp_pkt_t*)rx_queue[i].data;
            if (ntohs(r->ethertype) == ETH_TYPE_ARP &&
                ntohs(r->oper) == 2 &&
                r->spa == ip) {
                arp_cache(ip, r->sha);
                kmemcpy(out_mac, r->sha, 6);
                rx_queue[i].used = false;
                return 0;
            }
            rx_queue[i].used = false;
        }
    }
    return -1; /* timeout */
}

void arp_announce(void) {
    /* Gratuitous ARP: announce our IP to the network */
    arp_pkt_t pkt;
    kmemset(&pkt, 0, sizeof(pkt));
    kmemset(pkt.dst_mac, 0xFF, 6);
    kmemcpy(pkt.src_mac, cfg.mac, 6);
    pkt.ethertype = htons(ETH_TYPE_ARP);
    pkt.htype = htons(1);
    pkt.ptype = htons(ETH_TYPE_IP);
    pkt.hlen  = 6; pkt.plen = 4;
    pkt.oper  = htons(2); /* reply */
    kmemcpy(pkt.sha, cfg.mac, 6);
    pkt.spa = cfg.ip;
    kmemset(pkt.tha, 0xFF, 6);
    pkt.tpa = cfg.ip;
    net_send((uint8_t*)&pkt, sizeof(pkt));
}

/* ── ICMP / Ping ─────────────────────────────────── */

typedef struct __attribute__((packed)) {
    /* Ethernet */
    uint8_t  dst_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
    /* IPv4 */
    uint8_t  ver_ihl;
    uint8_t  dscp;
    uint16_t total_len;
    uint16_t ident;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t ip_csum;
    uint32_t src_ip;
    uint32_t dst_ip;
    /* ICMP */
    uint8_t  icmp_type;
    uint8_t  icmp_code;
    uint16_t icmp_csum;
    uint16_t icmp_id;
    uint16_t icmp_seq;
    uint8_t  payload[32];
} icmp_echo_pkt_t;

ping_result_t net_ping(ip_addr_t dest, uint8_t ttl) {
    ping_result_t res = {false, 0};
    if (!cfg.link_up) return res;

    mac_addr_t dst_mac;
    /* Determine if dest is on-subnet or use gateway */
    ip_addr_t via = ((dest & cfg.netmask) == (cfg.ip & cfg.netmask))
                    ? dest : cfg.gateway;

    if (arp_resolve(via, dst_mac) < 0) return res;

    icmp_echo_pkt_t pkt;
    kmemset(&pkt, 0, sizeof(pkt));

    /* Ethernet */
    kmemcpy(pkt.dst_mac, dst_mac, 6);
    kmemcpy(pkt.src_mac, cfg.mac, 6);
    pkt.ethertype = htons(ETH_TYPE_IP);

    /* IPv4 */
    pkt.ver_ihl   = 0x45;
    pkt.total_len = htons(sizeof(icmp_echo_pkt_t) - ETH_HDR_LEN);
    pkt.ident     = htons(0x4150); /* "AP" */
    pkt.flags_frag= 0;
    pkt.ttl       = ttl;
    pkt.proto     = IP_PROTO_ICMP;
    pkt.src_ip    = cfg.ip;
    pkt.dst_ip    = dest;
    pkt.ip_csum   = ip_checksum(&pkt.ver_ihl, 20);

    /* ICMP echo request */
    pkt.icmp_type = 8; /* echo request */
    pkt.icmp_code = 0;
    pkt.icmp_id   = htons(0x0001);
    pkt.icmp_seq  = htons(0x0001);
    /* payload magic */
    for (int i = 0; i < 32; i++) pkt.payload[i] = (uint8_t)i;
    pkt.icmp_csum = ip_checksum(&pkt.icmp_type, sizeof(pkt) - ETH_HDR_LEN - 20);

    uint32_t t0 = timer_get_ticks();
    net_send((uint8_t*)&pkt, sizeof(pkt));

    /* Wait up to 3 seconds for echo reply */
    for (int t = 0; t < 3000; t++) {
        timer_sleep(1);
        net_poll();
        for (int i = 0; i < NET_RX_BUFFERS; i++) {
            if (!rx_queue[i].used) continue;
            icmp_echo_pkt_t *r = (icmp_echo_pkt_t*)rx_queue[i].data;
            if (ntohs(r->ethertype) == ETH_TYPE_IP &&
                r->proto == IP_PROTO_ICMP &&
                r->icmp_type == 0 /* echo reply */ &&
                r->src_ip == dest) {
                res.reached = true;
                res.rtt_ms  = (timer_get_ticks() - t0) * 10; /* 100Hz ticks */
                rx_queue[i].used = false;
                return res;
            }
            rx_queue[i].used = false;
        }
    }
    return res; /* timed out */
}

/* ── UDP ─────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  dst_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
    uint8_t  ver_ihl;
    uint8_t  dscp;
    uint16_t total_len;
    uint16_t ident;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  proto;
    uint16_t ip_csum;
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t udp_len;
    uint16_t udp_csum;
} udp_hdr_t;

int udp_send(ip_addr_t dest, uint16_t sport, uint16_t dport,
             const uint8_t *payload, uint16_t plen) {
    if (!cfg.link_up) return -1;
    if (plen > NET_BUF_SIZE - sizeof(udp_hdr_t)) return -1;

    mac_addr_t dst_mac;
    ip_addr_t via = ((dest & cfg.netmask) == (cfg.ip & cfg.netmask))
                    ? dest : cfg.gateway;
    if (arp_resolve(via, dst_mac) < 0) return -1;

    /* Build frame in a stack buffer */
    static uint8_t frame[NET_BUF_SIZE];
    udp_hdr_t *h = (udp_hdr_t *)frame;
    kmemset(h, 0, sizeof(udp_hdr_t));

    kmemcpy(h->dst_mac, dst_mac, 6);
    kmemcpy(h->src_mac, cfg.mac, 6);
    h->ethertype  = htons(ETH_TYPE_IP);
    h->ver_ihl    = 0x45;
    uint16_t ip_total = (uint16_t)(20 + 8 + plen);
    h->total_len  = htons(ip_total);
    h->ident      = htons(0x4150);
    h->ttl        = 64;
    h->proto      = IP_PROTO_UDP;
    h->src_ip     = cfg.ip;
    h->dst_ip     = dest;
    h->ip_csum    = ip_checksum(&h->ver_ihl, 20);
    h->src_port   = htons(sport);
    h->dst_port   = htons(dport);
    h->udp_len    = htons((uint16_t)(8 + plen));
    h->udp_csum   = 0; /* optional */

    kmemcpy(frame + sizeof(udp_hdr_t), payload, plen);
    return net_send(frame, (uint16_t)(sizeof(udp_hdr_t) + plen));
}

/* ── Accessors ───────────────────────────────────── */
net_config_t *net_get_config(void) { return &cfg; }

void net_print_info(void) {
    char buf[64];
    vga_set_color(LIGHT_CYAN, BLACK);
    vga_puts("╔══════════════════════════════════════╗\n");
    vga_puts("║       ApexOS Network Status          ║\n");
    vga_puts("╠══════════════════════════════════════╣\n");
    vga_set_color(LIGHT_GREY, BLACK);

    vga_puts("  Status:  ");
    vga_set_color(cfg.link_up ? LIGHT_GREEN : LIGHT_RED, BLACK);
    vga_puts(cfg.link_up ? "LINK UP\n" : "NO LINK (simulated)\n");
    vga_set_color(LIGHT_GREY, BLACK);

    mac_to_str(cfg.mac, buf);
    vga_puts("  MAC:     "); vga_set_color(WHITE,BLACK); vga_puts(buf); vga_putchar('\n');
    vga_set_color(LIGHT_GREY, BLACK);

    ip_to_str(cfg.ip, buf);
    vga_puts("  IP:      "); vga_set_color(WHITE,BLACK); vga_puts(buf); vga_putchar('\n');
    vga_set_color(LIGHT_GREY, BLACK);

    ip_to_str(cfg.gateway, buf);
    vga_puts("  Gateway: "); vga_set_color(WHITE,BLACK); vga_puts(buf); vga_putchar('\n');
    vga_set_color(LIGHT_GREY, BLACK);

    ip_to_str(cfg.netmask, buf);
    vga_puts("  Netmask: "); vga_set_color(WHITE,BLACK); vga_puts(buf); vga_putchar('\n');
    vga_set_color(LIGHT_GREY, BLACK);

    ip_to_str(cfg.dns, buf);
    vga_puts("  DNS:     "); vga_set_color(WHITE,BLACK); vga_puts(buf); vga_putchar('\n');

    vga_set_color(LIGHT_CYAN, BLACK);
    vga_puts("╚══════════════════════════════════════╝\n");
    vga_set_color(LIGHT_GREY, BLACK);
}
