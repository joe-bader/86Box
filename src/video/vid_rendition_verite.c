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

#define ROM_SCREAMIN3D "roms/video/v1000/can76.rom"

#define VERITE_VRAM_SIZE (4 << 20)

typedef struct verite_t {
    svga_t        svga;
    rom_t         bios_rom;
    mem_mapping_t linear_mapping;
    mem_mapping_t mmio_mapping;
    uint8_t       pci_regs[256];
    uint8_t       pci_slot;
    uint8_t       int_line;
    uint32_t      linear_base;
    uint8_t       modereg;
    uint8_t       debugreg;
} verite_t;

static video_timings_t timing_verite = {
    .type = VIDEO_PCI,
    .write_b = 4,
    .write_w = 8,
    .write_l = 16,
    .read_b = 4,
    .read_w = 8,
    .read_l = 16
};

static void verite_out(uint16_t addr, uint8_t val, void *priv);
static uint8_t verite_in(uint16_t addr, void *priv);
static void verite_recalcmapping(verite_t *dev);

static uint8_t
verite_mmio_read(uint32_t addr, void *priv)
{
    verite_t *dev = (verite_t *) priv;
    uint8_t ret = 0;

    addr &= 0xfff;

    switch (addr) {
        case 0x72:
            ret = dev->modereg;
            break;
        case 0x48:
            ret = dev->debugreg;
            break;
        default:
            break;
    }

    return ret;
}

static void
verite_mmio_write(uint32_t addr, uint8_t val, void *priv)
{
    verite_t *dev = (verite_t *) priv;

    addr &= 0xfff;

    switch (addr) {
        case 0x48:
            dev->debugreg = val;
            if (val & 0x01) {
                dev->modereg = 0x02;
            }
            break;
        case 0x72:
            dev->modereg = val;
            break;
        default:
            break;
    }
}

static uint16_t
verite_mmio_read_w(uint32_t addr, void *priv)
{
    return verite_mmio_read(addr, priv) | (verite_mmio_read(addr + 1, priv) << 8);
}

static uint32_t
verite_mmio_read_l(uint32_t addr, void *priv)
{
    return verite_mmio_read(addr, priv) | (verite_mmio_read(addr + 1, priv) << 8) |
           (verite_mmio_read(addr + 2, priv) << 16) | (verite_mmio_read(addr + 3, priv) << 24);
}

static void
verite_mmio_write_w(uint32_t addr, uint16_t val, void *priv)
{
    verite_mmio_write(addr, val & 0xff, priv);
    verite_mmio_write(addr + 1, (val >> 8) & 0xff, priv);
}

static void
verite_mmio_write_l(uint32_t addr, uint32_t val, void *priv)
{
    verite_mmio_write(addr, val & 0xff, priv);
    verite_mmio_write(addr + 1, (val >> 8) & 0xff, priv);
    verite_mmio_write(addr + 2, (val >> 16) & 0xff, priv);
    verite_mmio_write(addr + 3, (val >> 24) & 0xff, priv);
}

static uint8_t
verite_pci_read(int func, int addr, int len, void *priv)
{
    verite_t *dev = (verite_t *) priv;

    (void) func;
    (void) len;

    switch (addr) {
        case 0x00: return 0x63;
        case 0x01: return 0x11;
        case 0x02: return 0x01;
        case 0x03: return 0x00;
        case PCI_REG_COMMAND: return dev->pci_regs[PCI_REG_COMMAND];
        case 0x07: return 0x02;
        case 0x08: return 0x00;
        case 0x09: return 0x00;
        case 0x0a: return 0x00;
        case 0x0b: return 0x03;
        case 0x10: return 0x00;
        case 0x11: return 0x00;
        case 0x12: return dev->linear_base >> 16;
        case 0x13: return dev->linear_base >> 24;
        case 0x30: return dev->pci_regs[0x30];
        case 0x31: return 0x00;
        case 0x32: return dev->pci_regs[0x32];
        case 0x33: return dev->pci_regs[0x33];
        case 0x3c: return dev->int_line;
        case 0x3d: return PCI_INTA;
        default: return 0;
    }
}

static void
verite_pci_write(int func, int addr, int len, uint8_t val, void *priv)
{
    verite_t *dev = (verite_t *) priv;

    (void) func;
    (void) len;

    switch (addr) {
        case PCI_REG_COMMAND:
            dev->pci_regs[PCI_REG_COMMAND] = val & 0x23;
            verite_recalcmapping(dev);
            break;
        case 0x12:
            dev->linear_base = (dev->linear_base & 0xff000000) | ((val & 0xc0) << 16);
            verite_recalcmapping(dev);
            break;
        case 0x13:
            dev->linear_base = (dev->linear_base & 0x00c00000) | (val << 24);
            verite_recalcmapping(dev);
            break;
        case 0x30:
        case 0x32:
        case 0x33:
            dev->pci_regs[addr] = val;
            if (dev->pci_regs[0x30] & 0x01) {
                uint32_t bios_addr = (dev->pci_regs[0x32] << 16) | (dev->pci_regs[0x33] << 24);
                mem_mapping_set_addr(&dev->bios_rom.mapping, bios_addr, 0x8000);
            } else {
                mem_mapping_disable(&dev->bios_rom.mapping);
            }
            break;
        case 0x3c:
            dev->int_line = val;
            break;
        default:
            break;
    }
}

static void
verite_recalcmapping(verite_t *dev)
{
    mem_mapping_disable(&dev->linear_mapping);
    mem_mapping_disable(&dev->mmio_mapping);

    if (dev->pci_regs[PCI_REG_COMMAND] & PCI_COMMAND_MEM) {
        if (dev->linear_base) {
            mem_mapping_set_addr(&dev->mmio_mapping, dev->linear_base, 0x1000);
            mem_mapping_set_addr(&dev->linear_mapping, dev->linear_base + 0x1000, VERITE_VRAM_SIZE - 0x1000);
        }
    }
}

static void
verite_out(uint16_t addr, uint8_t val, void *priv)
{
    verite_t *dev = (verite_t *) priv;
    svga_out(addr, val, &dev->svga);
}

static uint8_t
verite_in(uint16_t addr, void *priv)
{
    verite_t *dev = (verite_t *) priv;
    return svga_in(addr, &dev->svga);
}

static void
verite_recalctimings(svga_t *svga)
{
}

static void *
verite_init(const device_t *info)
{
    verite_t *dev = malloc(sizeof(verite_t));
    memset(dev, 0, sizeof(verite_t));

    dev->modereg = 0x02;

    rom_init(&dev->bios_rom, ROM_SCREAMIN3D, 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

    video_inform(VIDEO_FLAG_TYPE_SPECIAL, &timing_verite);

    svga_init(info, &dev->svga, dev, VERITE_VRAM_SIZE, verite_recalctimings, verite_in, verite_out, NULL, NULL);

    dev->svga.packed_chain4 = 1;
    dev->svga.miscout = 1;
    dev->svga.bpp = 8;

    mem_mapping_add(&dev->mmio_mapping, 0, 0,
                    verite_mmio_read, verite_mmio_read_w, verite_mmio_read_l,
                    verite_mmio_write, verite_mmio_write_w, verite_mmio_write_l,
                    NULL, MEM_MAPPING_EXTERNAL, dev);
    mem_mapping_disable(&dev->mmio_mapping);

    mem_mapping_add(&dev->linear_mapping, 0, 0,
                    svga_read_linear, svga_readw_linear, svga_readl_linear,
                    svga_write_linear, svga_writew_linear, svga_writel_linear,
                    NULL, MEM_MAPPING_EXTERNAL, &dev->svga);
    mem_mapping_disable(&dev->linear_mapping);

    io_sethandler(0x03c0, 0x0020, verite_in, NULL, NULL, verite_out, NULL, NULL, dev);

    pci_add_card(PCI_ADD_NORMAL, verite_pci_read, verite_pci_write, dev, &dev->pci_slot);

    dev->pci_regs[PCI_REG_COMMAND] = 0x03;
    dev->pci_regs[0x30] = 0x00;
    dev->pci_regs[0x32] = 0x0c;
    dev->pci_regs[0x33] = 0x00;

    svga_recalctimings(&dev->svga);

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
