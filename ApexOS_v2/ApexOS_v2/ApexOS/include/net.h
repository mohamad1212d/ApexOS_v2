/* =====================================================
   include/net.h - ApexOS Network Stack
   NE2000 / RTL8139 compatible Ethernet driver
   + ARP, IP, ICMP (ping), UDP primitives
===================================================== */
#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── PCI / IO constants ─────────────────────────── */
/* NE2000 default IO base (can be overridden by PCI scan) */
#define NE2000_IO_BASE   0x300

/* RTL8139 PCI Vendor/Device IDs */
#define RTL8139_VENDOR   0x10EC
#define RTL8139_DEVICE   0x8139

/* ── Ethernet frame limits ───────────────────────── */
#define ETH_FRAME_MAX    1518
#define ETH_FRAME_MIN      64
#define ETH_HDR_LEN        14

/* ── Ethertype values ────────────────────────────── */
#define ETH_TYPE_ARP   0x0806
#define ETH_TYPE_IP    0x0800

/* ── IP protocol numbers ─────────────────────────── */
#define IP_PROTO_ICMP  1
#define IP_PROTO_UDP   17
#define IP_PROTO_TCP   6

/* ── MAC / IP address types ──────────────────────── */
typedef uint8_t  mac_addr_t[6];
typedef uint32_t ip_addr_t;      /* network byte order */

/* ── Network configuration ───────────────────────── */
typedef struct {
    mac_addr_t mac;           /* our MAC address */
    ip_addr_t  ip;            /* our IP (network order) */
    ip_addr_t  gateway;       /* default gateway */
    ip_addr_t  netmask;       /* subnet mask */
    ip_addr_t  dns;           /* DNS server */
    bool       link_up;       /* is link up? */
    bool       initialized;   /* driver ready? */
} net_config_t;

/* ── Packet buffer ───────────────────────────────── */
#define NET_RX_BUFFERS   4
#define NET_TX_BUFFERS   4
#define NET_BUF_SIZE  1536

typedef struct {
    uint8_t  data[NET_BUF_SIZE];
    uint16_t len;
    bool     used;
} net_packet_t;

/* ── ARP table ───────────────────────────────────── */
#define ARP_TABLE_SIZE  16

typedef struct {
    ip_addr_t  ip;
    mac_addr_t mac;
    bool       valid;
} arp_entry_t;

/* ── ICMP echo (ping) result ────────────────────── */
typedef struct {
    bool    reached;
    uint32_t rtt_ms;   /* round-trip time in ms */
} ping_result_t;

/* ═══════════════════════════════════════════════════
   Public API
═══════════════════════════════════════════════════ */

/* Initialise network (probes PCI for RTL8139, falls back to NE2000 sim) */
int  net_init(void);

/* Send a raw Ethernet frame */
int  net_send(const uint8_t *frame, uint16_t len);

/* Poll for received frames (call from shell/timer) */
int  net_poll(void);

/* ARP */
int  arp_resolve(ip_addr_t ip, mac_addr_t out_mac);   /* fills out_mac */
void arp_announce(void);                               /* gratuitous ARP */

/* IP helpers */
ip_addr_t ip_from_str(const char *s);                 /* "192.168.1.1" -> uint32 */
void      ip_to_str(ip_addr_t ip, char *out);         /* uint32 -> "x.x.x.x" */
void      mac_to_str(const mac_addr_t mac, char *out);/* MAC -> "xx:xx:xx:xx:xx:xx" */

/* ICMP */
ping_result_t net_ping(ip_addr_t dest, uint8_t ttl);

/* UDP */
int  udp_send(ip_addr_t dest, uint16_t sport, uint16_t dport,
              const uint8_t *payload, uint16_t plen);

/* Config access */
net_config_t *net_get_config(void);
void          net_print_info(void);    /* prints to VGA */

/* Checksum */
uint16_t ip_checksum(const void *data, uint16_t len);

#endif /* NET_H */
