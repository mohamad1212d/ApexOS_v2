# ApexOS v2.0

A lightweight x86 32-bit operating system written in C and Assembly.

## What's New in v2.0

### Enhanced Filesystem (ApexFS)
- Hierarchical directory tree with full path support
- `cd`, `pwd` — navigate directories
- `rm` — delete files and empty directories
- `cp` — copy files
- `mv` — move/rename files
- `stat` — file metadata and permissions
- `tree` — recursive directory listing
- File timestamps, size tracking, rwx permissions
- Up to 64 inodes, 1KB content per file

### Network Stack
- **RTL8139 Ethernet driver** (auto-detected via PCI scan)
- **ARP** — address resolution with cache table
- **IP/ICMP** — `ping` command with RTT measurement
- **UDP** — send datagrams to any IP:port
- Graceful fallback if no NIC found (simulated mode)
- Commands: `ifconfig`, `net`, `ping`, `arp`

## Building

### Requirements
```bash
sudo apt install nasm gcc binutils grub-pc-bin grub-common xorriso
```

### Build
```bash
cd ApexOS
chmod +x build.sh
./build.sh
```
Output: `ApexOS.iso`

## Running

### QEMU (recommended)
```bash
# Basic (no network)
qemu-system-i386 -cdrom ApexOS.iso -m 32M

# With RTL8139 network
qemu-system-i386 -cdrom ApexOS.iso -m 32M \
    -net nic,model=rtl8139 -net user
```

### VirtualBox
1. New VM → Other/Unknown (32-bit)
2. Attach `ApexOS.iso` as optical disk
3. Network: Adapter 1 → PCNet-FAST III (RTL8139 compatible)

## Commands

| Command | Description |
|---------|-------------|
| `ls [path]` | List directory |
| `cd <path>` | Change directory |
| `pwd` | Print working directory |
| `mkdir <dir>` | Create directory |
| `touch <file>` | Create file |
| `cat <file>` | Show file |
| `write <file>` | Write to file |
| `rm <path>` | Remove file/empty dir |
| `cp <src> <dst>` | Copy file |
| `mv <src> <dst>` | Move/rename |
| `stat <path>` | File metadata |
| `tree [path]` | Directory tree |
| `ifconfig` | Network interface info |
| `net` | Network config |
| `ping <ip>` | ICMP echo |
| `arp` | ARP table |
| `sysinfo` | System info |
| `uptime` | System uptime |
| `echo <text>` | Print text |
| `lang` | Toggle Arabic/English |
| `clear` | Clear screen |
| `reboot` | Reboot |

## Architecture

```
ApexOS/
├── boot/           # Multiboot, GDT flush, ISR stubs (ASM)
├── kernel/         # kernel.c, gdt.c, idt.c, memory.c
│   ├── fs.c        # NEW: ApexFS hierarchical filesystem
│   └── boot_anim.c
├── drivers/
│   ├── vga.c       # VGA text mode
│   ├── keyboard.c  # PS/2 keyboard (AR+EN)
│   ├── timer.c     # PIT 100Hz
│   └── net.c       # NEW: RTL8139 + ARP + IP + ICMP + UDP
├── shell/
│   └── shell.c     # Interactive shell
├── include/
│   ├── apex.h      # Main header
│   ├── fs.h        # NEW: Filesystem API
│   └── net.h       # NEW: Network API
└── build.sh
```
