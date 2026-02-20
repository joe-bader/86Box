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
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define HAVE_STDARG_H
#include <86box/86box.h>
#include <86box/device.h>
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/pci.h>
#include <86box/rom.h>
#include <86box/timer.h>
#include <86box/video.h>
#include <86box/vid_svga.h>
#include <86box/vid_rendition_verite.h>

#define ROM_SCREAMIN3D "roms/video/v1000/can76.rom"

static video_timings_t timing_verite = {
    .type = VIDEO_PCI,
    .write_b = 2,
    .write_w = 2,
    .write_l = 4,
    .read_b = 20,
    .read_w = 20,
    .read_l = 32
};

static void verite_recalctimings(svga_t *svga);
static void verite_out(uint16_t addr, uint8_t val, void *priv);
static uint8_t verite_in(uint16_t addr, void *priv);
static void verite_updatemapping(verite_t *dev);
static void verite_update_rom_mapping(verite_t *dev);

static void
verite_update_rom_mapping(verite_t *dev)
{
    uint32_t bios_addr = (dev->pci_regs[0x32] << 16) | (dev->pci_regs[0x33] << 24);
    if ((dev->pci_regs[0x30] & 0x01) && bios_addr) {
        mem_mapping_set_addr(&dev->bios_rom.mapping, bios_addr, 0x8000);
    } else {
        mem_mapping_disable(&dev->bios_rom.mapping);
    }
}

static void
verite_io_set(verite_t *dev)
{
    io_removehandler(0x03c0, 0x0020, verite_in, NULL, NULL, verite_out, NULL, NULL, dev);
    io_sethandler(0x03c0, 0x0020, verite_in, NULL, NULL, verite_out, NULL, NULL, dev);
}

static void
verite_io_remove(verite_t *dev)
{
    io_removehandler(0x03c0, 0x0020, verite_in, NULL, NULL, verite_out, NULL, NULL, dev);
}

static void
verite_risc_reset(verite_t *dev)
{
    memset(&dev->risc, 0, sizeof(dev->risc));
    dev->risc.regs[0] = 0;
    dev->risc.halted = 1;
}

static void
verite_risc_init(verite_t *dev)
{
    verite_risc_reset(dev);
    for (int i = 0; i < 32; i++) {
        dev->risc.regs[i] = 0;
    }
    dev->risc.regs[32] = 0;
    dev->risc.regs[48] = 0xffffffff;
    dev->risc.regs[49] = 0x00ff00ff;
    dev->risc.regs[50] = 0xff00ff00;
    dev->risc.regs[51] = 0x0000ffff;
    dev->risc.regs[52] = 0xffff0000;
    dev->risc.regs[53] = 0x000000ff;
    dev->risc.regs[54] = 0x0000ff00;
    dev->risc.regs[55] = 0x00ff0000;
    dev->risc.regs[56] = 0xff000000;
    dev->risc.regs[57] = 0x00ffff00;
    dev->risc.regs[58] = 0xff00ff00;
    dev->risc.regs[59] = 0xffffff00;
    dev->risc.regs[60] = 0x0000ffff;
    dev->risc.regs[61] = 0x00ffffff;
    dev->risc.regs[62] = 0xff00ffff;
    dev->risc.regs[63] = 0xffff00ff;
}

static uint8_t
verite_pci_read(int func, int addr, int len, void *priv)
{
    verite_t *dev = (verite_t *) priv;
    uint8_t   ret = 0;

    (void) func;
    (void) len;

    switch (addr) {
        case 0x00:
            ret = VERITE_VENDOR_ID & 0xff;
            break;
        case 0x01:
            ret = (VERITE_VENDOR_ID >> 8) & 0xff;
            break;
        case 0x02:
            ret = VERITE_DEVICE_ID & 0xff;
            break;
        case 0x03:
            ret = (VERITE_DEVICE_ID >> 8) & 0xff;
            break;
        case PCI_REG_COMMAND:
            ret = dev->pci_regs[PCI_REG_COMMAND];
            break;
        case PCI_REG_COMMAND_H:
            ret = dev->pci_regs[PCI_REG_COMMAND_H];
            break;
        case 0x06:
            ret = dev->pci_regs[0x06];
            break;
        case 0x07:
            ret = dev->pci_regs[0x07];
            break;
        case 0x08:
            ret = 0x00;
            break;
        case 0x09:
            ret = 0x00;
            break;
        case 0x0a:
            ret = 0x00;
            break;
        case 0x0b:
            ret = 0x03;
            break;
        case 0x0c:
            ret = dev->pci_regs[0x0c];
            break;
        case 0x0d:
            ret = dev->pci_regs[0x0d];
            break;
        case 0x0e:
            ret = 0x00;
            break;
        case 0x0f:
            ret = 0x00;
            break;
        case 0x10:
            ret = dev->mem_base & 0xff;
            break;
        case 0x11:
            ret = (dev->mem_base >> 8) & 0xff;
            break;
        case 0x12:
            ret = (dev->mem_base >> 16) & 0xff;
            break;
        case 0x13:
            ret = (dev->mem_base >> 24) & 0xff;
            break;
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            ret = 0x00;
            break;
        case 0x30:
            ret = dev->pci_regs[0x30] & 0x01;
            break;
        case 0x31:
            ret = 0x00;
            break;
        case 0x32:
            ret = dev->pci_regs[0x32];
            break;
        case 0x33:
            ret = dev->pci_regs[0x33];
            break;
        case 0x3c:
            ret = dev->int_line;
            break;
        case 0x3d:
            ret = PCI_INTA;
            break;
        default:
            ret = dev->pci_regs[addr];
            break;
    }

    return ret;
}

static void
verite_pci_write(int func, int addr, int len, uint8_t val, void *priv)
{
    verite_t *dev = (verite_t *) priv;

    (void) func;
    (void) len;

    switch (addr) {
        case PCI_REG_COMMAND:
            dev->pci_regs[PCI_REG_COMMAND] = val & 0x27;
            verite_updatemapping(dev);
            break;
        case PCI_REG_COMMAND_H:
            dev->pci_regs[PCI_REG_COMMAND_H] = val & 0x03;
            break;
        case 0x06:
            dev->pci_regs[0x06] = val & 0xf0;
            break;
        case 0x07:
            dev->pci_regs[0x07] = val & 0x01;
            break;
        case 0x0c:
            dev->pci_regs[0x0c] = val;
            break;
        case 0x0d:
            dev->pci_regs[0x0d] = val & 0xff;
            break;
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            if (addr == 0x10)
                dev->mem_base = (dev->mem_base & 0xffffff00) | (val & 0x00);
            else if (addr == 0x11)
                dev->mem_base = (dev->mem_base & 0xffff00ff) | ((val & 0x00) << 8);
            else if (addr == 0x12)
                dev->mem_base = (dev->mem_base & 0xff00ffff) | ((val & 0xc0) << 16);
            else if (addr == 0x13)
                dev->mem_base = (dev->mem_base & 0x00ffffff) | (val << 24);
            dev->mem_base &= 0xffc00000;
            verite_updatemapping(dev);
            break;
        case 0x30:
            dev->pci_regs[0x30] = val & 0x01;
            verite_update_rom_mapping(dev);
            break;
        case 0x31:
            break;
        case 0x32:
            dev->pci_regs[0x32] = val;
            verite_update_rom_mapping(dev);
            break;
        case 0x33:
            dev->pci_regs[0x33] = val;
            verite_update_rom_mapping(dev);
            break;
        case 0x3c:
            dev->int_line = val;
            break;
        default:
            break;
    }
}

static void
verite_updatemapping(verite_t *dev)
{
    svga_t *svga = &dev->svga;

    mem_mapping_disable(&dev->linear_mapping);

    if (dev->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_MEM) {
        if (dev->mem_base) {
            mem_mapping_set_addr(&dev->linear_mapping, dev->mem_base, VERITE_VRAM_SIZE);
        }
    }

    if (dev->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_IO) {
        verite_io_set(dev);
    } else {
        verite_io_remove(dev);
    }

    if (dev->in_vga_mode) {
        mem_mapping_enable(&svga->mapping);
    } else {
        mem_mapping_disable(&svga->mapping);
    }
}

static void
verite_out(uint16_t addr, uint8_t val, void *priv)
{
    verite_t *dev = (verite_t *) priv;
    svga_t   *svga = &dev->svga;

    if (!dev->in_vga_mode) {
        if (addr >= 0x3c0 && addr < 0x3e0) {
            if ((addr >= 0x3c8) && (addr <= 0x3cf)) {
                switch (addr) {
                    case 0x3c8:
                        svga->dac_addr = val;
                        svga->dac_pos = 0;
                        break;
                    case 0x3c9:
                        bt48x_ramdac_out(addr - 0x3c0, 0, 0, val, dev->ramdac, svga);
                        break;
                    default:
                        break;
                }
            }
        }
        return;
    }

    svga_out(addr, val, svga);
}

static uint8_t
verite_in(uint16_t addr, void *priv)
{
    verite_t *dev = (verite_t *) priv;
    svga_t   *svga = &dev->svga;
    uint8_t   ret = 0xff;

    if (!dev->in_vga_mode) {
        if (addr >= 0x3c0 && addr < 0x3e0) {
            if ((addr >= 0x3c8) && (addr <= 0x3cf)) {
                switch (addr) {
                    case 0x3c8:
                        ret = svga->dac_addr;
                        break;
                    case 0x3c9:
                        ret = bt48x_ramdac_in(addr - 0x3c0, 0, 0, dev->ramdac, svga);
                        break;
                    default:
                        break;
                }
            }
        }
        return ret;
    }

    return svga_in(addr, svga);
}

static void
verite_recalctimings(svga_t *svga)
{
    verite_t *dev = (verite_t *) svga->priv;

    if (!dev->in_vga_mode) {
        return;
    }

    bt48x_recalctimings(dev->ramdac, svga);
}

static uint8_t
verite_read_linear(uint32_t addr, void *priv)
{
    verite_t *dev = (verite_t *) priv;
    svga_t   *svga = &dev->svga;

    addr &= svga->vram_mask;

    return svga->vram[addr];
}

static uint16_t
verite_readw_linear(uint32_t addr, void *priv)
{
    verite_t *dev = (verite_t *) priv;
    svga_t   *svga = &dev->svga;

    addr &= svga->vram_mask;

    return *(uint16_t *) &svga->vram[addr];
}

static uint32_t
verite_readl_linear(uint32_t addr, void *priv)
{
    verite_t *dev = (verite_t *) priv;
    svga_t   *svga = &dev->svga;

    addr &= svga->vram_mask;

    return *(uint32_t *) &svga->vram[addr];
}

static void
verite_write_linear(uint32_t addr, uint8_t val, void *priv)
{
    verite_t *dev = (verite_t *) priv;
    svga_t   *svga = &dev->svga;

    addr &= svga->vram_mask;

    svga->vram[addr] = val;
    svga->changedvram[addr >> 12] = changeframecount;
}

static void
verite_writew_linear(uint32_t addr, uint16_t val, void *priv)
{
    verite_t *dev = (verite_t *) priv;
    svga_t   *svga = &dev->svga;

    addr &= svga->vram_mask;

    *(uint16_t *) &svga->vram[addr] = val;
    svga->changedvram[addr >> 12] = changeframecount;
}

static void
verite_writel_linear(uint32_t addr, uint32_t val, void *priv)
{
    verite_t *dev = (verite_t *) priv;
    svga_t   *svga = &dev->svga;

    addr &= svga->vram_mask;

    *(uint32_t *) &svga->vram[addr] = val;
    svga->changedvram[addr >> 12] = changeframecount;
}

static void *
verite_init(const device_t *info)
{
    verite_t *dev = malloc(sizeof(verite_t));
    memset(dev, 0, sizeof(verite_t));

    dev->vram_size = VERITE_VRAM_SIZE;
    dev->vram_mask = dev->vram_size - 1;
    dev->in_vga_mode = 1;

    rom_init(&dev->bios_rom, ROM_SCREAMIN3D, 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);
    mem_mapping_disable(&dev->bios_rom.mapping);

    svga_init(info, &dev->svga, dev, dev->vram_size,
              verite_recalctimings, verite_in, verite_out,
              NULL, NULL);

    dev->svga.decode_mask = dev->vram_mask;
    dev->svga.vram_mask = dev->vram_mask;
    dev->svga.vram_max = dev->vram_size;
    dev->svga.miscout = 1;
    dev->svga.bpp = 8;

    dev->ramdac = device_add(&bt485_ramdac_device);
    dev->svga.ramdac = dev->ramdac;
    dev->svga.dac_hwcursor_draw = bt48x_hwcursor_draw;

    mem_mapping_add(&dev->linear_mapping, 0, 0,
                    verite_read_linear, verite_readw_linear, verite_readl_linear,
                    verite_write_linear, verite_writew_linear, verite_writel_linear,
                    NULL, MEM_MAPPING_EXTERNAL, dev);
    mem_mapping_disable(&dev->linear_mapping);

    dev->pci_regs[PCI_REG_COMMAND] = PCI_COMMAND_IO | PCI_COMMAND_MEM;

    dev->pci_regs[0x30] = 0x00;
    dev->pci_regs[0x32] = 0x0c;
    dev->pci_regs[0x33] = 0x00;

    pci_add_card(PCI_ADD_NORMAL, verite_pci_read, verite_pci_write, dev, &dev->pci_slot);

    verite_io_set(dev);

    verite_risc_init(dev);

    video_inform(VIDEO_FLAG_TYPE_SPECIAL, &timing_verite);

    return dev;
}

static void
verite_close(void *priv)
{
    verite_t *dev = (verite_t *) priv;

    svga_close(&dev->svga);

    free(dev);
}

static void
verite_speed_changed(void *priv)
{
    verite_t *dev = (verite_t *) priv;

    svga_recalctimings(&dev->svga);
}

static void
verite_force_redraw(void *priv)
{
    verite_t *dev = (verite_t *) priv;

    dev->svga.fullchange = changeframecount;
}

static int
verite_available(void)
{
    return rom_present(ROM_SCREAMIN3D);
}

const device_t screamin3d_device = {
    .name = "Sierra Screamin' 3D",
    .internal_name = "screamin3d",
    .flags = DEVICE_PCI,
    .local = 0,
    .init = verite_init,
    .close = verite_close,
    .reset = NULL,
    .available = verite_available,
    .speed_changed = verite_speed_changed,
    .force_redraw = verite_force_redraw,
    .config = NULL
};
