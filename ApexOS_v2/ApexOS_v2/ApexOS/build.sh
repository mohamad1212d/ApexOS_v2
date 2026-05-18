#!/bin/bash
# =====================================================
#  build.sh - ApexOS Build Script
#  Builds kernel and creates bootable ISO
# =====================================================

set -e   # Stop on any error

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${CYAN}"
echo "  ___                 ___  ___ "
echo " / _ \\ ___  ___ __ __/ _ \\/ __/"
echo "/ __ |/ _ \\/ -_)\\ \\ / // /\\ \\  "
echo "/_/ |_/ .__/\\__//_/\\_/\\___/___/ v1.0"
echo "      /_/  Build System"
echo -e "${NC}"

# ── Check dependencies ─────────────────────────────
echo -e "${YELLOW}[1/5] Checking dependencies...${NC}"

check_dep(){
    if ! command -v "$1" &>/dev/null; then
        echo -e "${RED}Missing: $1${NC}"
        echo "Install with: sudo apt install $2"
        exit 1
    fi
    echo -e "  ${GREEN}✓${NC} $1"
}

check_dep nasm       "nasm"
check_dep gcc        "gcc"
check_dep ld         "binutils"
check_dep grub-mkrescue "grub-pc-bin grub-common"
check_dep xorriso    "xorriso"

# ── Setup directories ──────────────────────────────
echo -e "${YELLOW}[2/5] Setting up build directories...${NC}"
mkdir -p build/obj isodir/boot/grub
echo -e "  ${GREEN}✓${NC} Directories ready"

# ── Compile Assembly files ─────────────────────────
echo -e "${YELLOW}[3/5] Assembling...${NC}"

nasm -f elf32 boot/boot.s       -o build/obj/boot.o
echo -e "  ${GREEN}✓${NC} boot.s"

nasm -f elf32 boot/gdt_flush.s  -o build/obj/gdt_flush.o
echo -e "  ${GREEN}✓${NC} gdt_flush.s"

nasm -f elf32 boot/isr_stubs.s  -o build/obj/isr_stubs.o
echo -e "  ${GREEN}✓${NC} isr_stubs.s"

# ── Compile C files ────────────────────────────────
echo -e "${YELLOW}[4/5] Compiling C sources...${NC}"

CFLAGS="-m32 -std=c99 -ffreestanding -O2 -Wall -Wextra \
        -fno-builtin -fno-stack-protector -nostdlib \
        -Iinclude"

compile(){
    gcc $CFLAGS -c "$1" -o "build/obj/$2"
    echo -e "  ${GREEN}✓${NC} $1"
}

compile kernel/kernel.c      kernel.o
compile kernel/gdt.c         gdt.o
compile kernel/idt.c         idt.o
compile kernel/memory.c      memory.o
compile kernel/boot_anim.c   boot_anim.o
compile drivers/vga.c        vga.o
compile drivers/timer.c      timer.o
compile drivers/keyboard.c   keyboard.o
compile kernel/fs.c          fs.o
compile drivers/net.c        net.o
compile shell/shell.c        shell.o

# ── Link kernel ────────────────────────────────────
echo -e "${YELLOW}[5/5] Linking & building ISO...${NC}"

ld -m elf_i386 -T linker.ld -nostdlib \
    build/obj/boot.o \
    build/obj/gdt_flush.o \
    build/obj/isr_stubs.o \
    build/obj/kernel.o \
    build/obj/gdt.o \
    build/obj/idt.o \
    build/obj/memory.o \
    build/obj/boot_anim.o \
    build/obj/vga.o \
    build/obj/timer.o \
    build/obj/keyboard.o \
    build/obj/fs.o \
    build/obj/net.o \
    build/obj/shell.o \
    -o isodir/boot/apex.kernel

echo -e "  ${GREEN}✓${NC} Kernel linked"

# Copy GRUB config
cp grub/grub.cfg isodir/boot/grub/grub.cfg

# Build ISO
grub-mkrescue -o ApexOS.iso isodir

echo -e "${GREEN}  Version 2.0 — FS + Network added${NC}" 2>/dev/null
echo -e "  ${GREEN}✓${NC} ISO created: ApexOS.iso"

# ── Done ───────────────────────────────────────────
echo ""
echo -e "${GREEN}╔═══════════════════════════════════════╗${NC}"
echo -e "${GREEN}║      Build Successful! 🎉              ║${NC}"
echo -e "${GREEN}╠═══════════════════════════════════════╣${NC}"
echo -e "${GREEN}║  Output: ApexOS.iso                   ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════╝${NC}"
echo ""
echo -e "${CYAN}Run in QEMU:${NC}"
echo "  qemu-system-i386 -cdrom ApexOS.iso -m 32M"
echo ""
echo -e "${CYAN}Run in VirtualBox:${NC}"
echo "  Create VM > Other/Unknown > Use ApexOS.iso as disk"
echo ""
echo -e "${CYAN}Write to USB (Linux):${NC}"
echo "  sudo dd if=ApexOS.iso of=/dev/sdX bs=4M status=progress"
echo ""
