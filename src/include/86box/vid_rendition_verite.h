/*
 * 86Box    A hypervisor and IBM PC system emulator that specializes in
 *          running old operating systems and software designed for IBM
 *          PC systems and compatibles from 1981 through fairly recent
 *          system designs based on the PCI bus.
 *
 *          This file is part of the 86Box distribution.
 *
 *          Rendition Verite V1000 (Sierra Screamin' 3D) video card emulation.
 *
 * Authors: 
 *
 *          Copyright 2025 
 */
#ifndef VID_RENDITION_VERITE_H
#define VID_RENDITION_VERITE_H

#include <86box/vid_svga.h>

#define VERITE_VRAM_SIZE (4 << 20)

#define VERITE_FIFO_SIZE 32

#define VERITE_ICACHE_SIZE    2048
#define VERITE_ICACHE_LINESIZE 32

#define VERITE_VENDOR_ID 0x1163
#define VERITE_DEVICE_ID 0x0001

#define FIFO_SWAP_NO  0x00
#define FIFO_SWAP_END 0x04
#define FIFO_SWAP_INHW 0x08
#define FIFO_SWAP_HW   0x0c
#define FIFOINFREE     0x40
#define FIFOOUTVALID   0x41
#define COMM           0x42
#define MEMENDIAN      0x43
#define INTR           0x44
#define INTREN         0x46
#define DEBUGREG       0x48
#define LOWWATERMARK   0x49
#define PCITEST        0x4c
#define DMACMDPTR      0x50
#define DMA_ADDRESS    0x54
#define DMA_COUNT      0x58
#define STATEINDEX     0x60
#define STATEDATA      0x64
#define SCRATCH        0x70
#define MODEREG        0x72
#define BANKSELECT     0x74
#define CRTCTEST       0x80
#define CRTCCTL        0x84
#define CRTCHORZ       0x88
#define CRTCVERT       0x8c
#define FRAMEBASEB     0x90
#define FRAMEBASEA     0x94
#define CRTCOFFSET     0x98
#define CRTCSTATUS     0x9c
#define DRAMCTL        0xa0
#define PALETTE        0xb0
#define RAMDACBASEADDR 0xb0
#define DEVICE0        0xc0
#define DEVICE1        0xd0

#define VERTINTR      0x01
#define FIFOLOWINTR   0x02
#define RISCINTR      0x04
#define HALTINTR      0x08
#define FIFOERRORINTR 0x10
#define DMAERRORINTR  0x20
#define DMAINTR       0x40
#define XINTR         0x80

#define VERTINTREN      0x01
#define FIFOLOWINTREN   0x02
#define RISCINTREN      0x04
#define HALTINTREN      0x08
#define FIFOERRORINTREN 0x10
#define DMAERRORINTREN  0x20
#define DMAINTREN       0x40
#define XINTREN         0x80

#define SOFTRESET    0x01
#define HOLDRISC     0x02
#define STEPRISC     0x04
#define DIRECTSCLK   0x08
#define SOFTVGARESET 0x10
#define SOFTXRESET   0x20

#define VESA_MODE 0x01
#define VGA_MODE  0x02
#define VGA_32    0x04
#define DMA_EN    0x08

#define DMABUSY 0x80

#define STATEINDEX_IR 128
#define STATEINDEX_PC 129
#define STATEINDEX_S1 130

#define CRTCTEST_NOTVBLANK 0x10000
#define CRTCTEST_VBLANK    0x40000

#define CRTCCTL_SCRNFMT_MASK       0x0f
#define CRTCCTL_VIDEOFIFOSIZE128   0x10
#define CRTCCTL_ENABLEDDC          0x20
#define CRTCCTL_DDCOUTPUT          0x40
#define CRTCCTL_DDCDATA            0x80
#define CRTCCTL_VSYNCHI            0x100
#define CRTCCTL_HSYNCHI            0x200
#define CRTCCTL_VSYNCENABLE        0x400
#define CRTCCTL_HSYNCENABLE        0x800
#define CRTCCTL_VIDEOENABLE        0x1000
#define CRTCCTL_STEREOSCOPIC       0x2000
#define CRTCCTL_FRAMEDISPLAYED     0x4000
#define CRTCCTL_FRAMEBUFFERBGR     0x8000
#define CRTCCTL_EVENFRAME          0x10000
#define CRTCCTL_LINEDOUBLE         0x20000
#define CRTCCTL_FRAMESWITCHED      0x40000

typedef struct verite_risc_t {
    uint32_t regs[256];
    uint32_t pc;
    uint32_t ir;
    uint32_t s1;
    int      halted;
    int      held;
    uint8_t  icache[VERITE_ICACHE_SIZE];
    uint32_t icache_addr;
    int      icache_valid;
} verite_risc_t;

typedef struct verite_t {
    svga_t        svga;
    rom_t         bios_rom;
    mem_mapping_t linear_mapping;
    mem_mapping_t mmio_mapping;
    
    uint8_t  pci_regs[256];
    uint8_t  pci_slot;
    uint32_t mem_base;
    uint32_t io_base;
    uint8_t  int_line;
    
    uint32_t fifo[VERITE_FIFO_SIZE];
    int      fifo_read;
    int      fifo_write;
    int      fifo_count;
    
    uint8_t  comm;
    uint8_t  memendian;
    uint8_t  intr;
    uint8_t  intren;
    uint8_t  debugreg;
    uint8_t  lowwatermark;
    uint32_t dmacmdptr;
    uint32_t dma_address;
    uint32_t dma_count;
    uint8_t  dma_busy;
    
    uint8_t  stateindex;
    uint32_t statedata;
    
    uint16_t scratch;
    uint8_t  modereg;
    uint32_t bankselect;
    
    uint32_t crtctest;
    uint32_t crtcctl;
    uint32_t crtchorz;
    uint32_t crtcvert;
    uint32_t framebasea;
    uint32_t framebaseb;
    uint32_t crtcoffset;
    
    uint32_t dramctl;
    uint32_t device0;
    uint32_t device1;
    
    uint8_t *vram;
    uint32_t vram_mask;
    int      vram_size;
    
    void *ramdac;
    
    verite_risc_t risc;
    
    int in_vga_mode;
} verite_t;

extern const device_t screamin3d_device;

#endif
