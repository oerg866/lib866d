/*  LIB866D
    PCI device configuration functions

    (C) 2024 E. Voirin (oerg866)
*/

#include "pci.h"

#include <stdio.h>
#include <dos.h>
#include <conio.h>
#include <malloc.h>
#include <assert.h>

#include "types.h"
#include "386asm.h"
#include "sys.h"

#define __LIB866D_TAG__ "PCI"
#include "debug.h"

static _inline bool pci_isDevice(pci_Device device) {
    return (pci_getVendorID(device) != 0xFFFF);
}

static _inline bool pci_isMultifunctionDevice(pci_Device device) {
    return (pci_isDevice(device) && (pci_read8(device, 0x0E) >> 7 > 0));
}

void pci_debugInfo(pci_Device device) {
    u8 header_type = pci_read8(device, 0x0EUL);
    printf("[%02x:%02x:%02x] [%04x:%04x] Header Type [%02x] %s\n",
        device.bus, device.slot, device.func,
        pci_getVendorID(device),
        pci_getDeviceID(device),
        header_type,
        header_type & 0x80UL ? "(Multi-Function)" : "");
}

u32 pci_read32(pci_Device device, u32 offset)
/* Reads DWORD from PCI config space. Assumes offset is DWORD-aligned. */
{
    u32 address = ((u32)device.bus << 16UL) | ((u32)device.slot << 11UL)
        | ((u32)device.func << 8UL) | (offset & 0xFCUL)
        | 0x80000000UL;
    sys_outPortL(0xCF8, address);
    return sys_inPortL(0xCFC);
}


u16 pci_read16(pci_Device device, u32 offset)
/* Reads WORD from PCI config space. Assumes offset is WORD-aligned. */
{
    return (offset & 0x02UL)    ? (u16) (pci_read32(device, offset) >> 16UL)
                                : (u16) (pci_read32(device, offset));
}

u8 pci_read8(pci_Device device, u32 offset)
/* Reads BYTE from PCI config space. */
{
    switch (offset & 3) {
    case 3: return (u8) (pci_read32(device, offset) >> 24UL);
    case 2: return (u8) (pci_read32(device, offset) >> 16UL);
    case 1: return (u8) (pci_read32(device, offset) >>  8UL);
    case 0: return (u8) (pci_read32(device, offset) >>  0UL);
    default: return 0; /* to silence the compiler warning... */
    }
}

void pci_readBytes(pci_Device device, u8 *buffer, u32 offset, u32 count) {
    u32 i;
    for (i = 0; i < count; i++) {
        buffer[i] = pci_read8(device, offset + i);
    }
}

void pci_write32(pci_Device device, u32 offset, u32 value)
/* Writes DWORD to PCI config space. Assumes offset is DWORD-aligned. */
{
    u32 address = ((u32)device.bus << 16UL) | ((u32)device.slot << 11UL)
        | ((u32)device.func << 8UL) | (offset & 0xFCUL)
        | 0x80000000UL;
    sys_outPortL(0xCF8, address);
    sys_outPortL(0xCFC, value);
}

void pci_write16(pci_Device device, u32 offset, u16 value) {
    u32 temp = pci_read32(device, offset);
    temp = (offset & 2) ? ((u32) value << 16) | (temp & 0xFFFF)
            : ((u32) value) | (temp << 16);
    pci_write32(device, offset, temp);
}

void pci_write8(pci_Device device, u32 offset, u8 value) {
    u32 temp = pci_read32(device, offset);
    switch (offset & 3) {
    case 3: temp = (temp & 0x00FFFFFFUL) | ((u32) value << 24UL); break;
    case 2: temp = (temp & 0xFF00FFFFUL) | ((u32) value << 16UL); break;
    case 1: temp = (temp & 0xFFFF00FFUL) | ((u32) value <<  8UL); break;
    case 0: temp = (temp & 0xFFFFFF00UL) | ((u32) value <<  0UL); break;
    }
    pci_write32(device, offset, temp);
}

u16 pci_getVendorID(pci_Device device) {
    return pci_read16(device, 0UL);
}

u16 pci_getDeviceID(pci_Device device) {
    return pci_read16(device, 2UL);
}

pci_Class pci_getClass(pci_Device device) {
    return (pci_Class) pci_read8(device, 0x0BUL);
}

u8 pci_getSubClass(pci_Device device) {
    return pci_read8(device, 0x0AUL);
}

bool pci_findDevByID(u16 ven, u16 dev, pci_Device *device) {
    pci_Device *current = NULL;

    while (NULL != (current = pci_getNextDevice(current))) {
        pci_debugInfo(*current);

        if (pci_getVendorID(*current) == ven && pci_getDeviceID(*current) == dev) {
            *device = *current;
            return true;
        }
    }

    return false;
}

pci_Device *pci_getNextDevice(pci_Device *device) {
    /* first iteration = call with NULL pointer, we will allocate,
       else base our search on the given device */
    if (device == NULL) {
        device = (pci_Device *)calloc(1, sizeof(pci_Device));
        assert(device != NULL);
    } else {
        goto restart; /* jump to the end of the for loop with existigng value */
    }

    for (device->bus = 0; device->bus < PCI_BUS_MAX; ++device->bus) {
        for (device->slot = 0; device->slot <= PCI_SLOT_MAX; ++device->slot) {
            for (device->func = 0; device->func <= PCI_FUNC_MAX; ++device->func) {
                if (pci_isDevice(*device)) return device;

/* Lord, for-give me for I have sinned */
restart:
                if (!pci_isMultifunctionDevice(*device)) break;
            }
        }
    }

    /* Last device was handled, no device found, dealloc and return NULL */
    free(device);
    return NULL;
}

/* Calculate size of a BAR. This is hard to do so it's a separate function*/
static u32 pci_getBARSize(pci_Device device, u32 index) {
    u32 oldBARAddress   = 0UL;
    u8  oldCommandByte  = 0x00;
    u32 pciBARReg       = 0x10UL + index * 4UL;
    u32 size            = 0UL;

    L866_ASSERT(index < 6);
    L866_ASSERT(pci_isDevice(device) == true);

    oldBARAddress = pci_read32(device, pciBARReg);
    oldCommandByte = pci_read8(device, 0x04);

    /* Disable IO and mem decode while we do this */
    pci_write8(device, 0x04UL, (u8) (oldCommandByte & 0xFC));

    /* Overwrite the entire BAR address with 1-bits and see what sticks */
    pci_write32(device, pciBARReg, 0xFFFFFFFFUL);
    size = pci_read32(device, pciBARReg);

    /* The inverse of that is our mask and thus the highest address in the BAR */
    size = ~size;

    /* + 1 = the actual size of the bar */
    size++;

    /* Write back old address and command byte to restore initial state */
    pci_write32(device, pciBARReg, oldBARAddress);
    pci_write8(device, 0x04, oldCommandByte);

    return size;
}

bool pci_populateDeviceInfo(pci_DeviceInfo *info, pci_Device device) {
    u32 i;

    L866_NULLCHECK(info);

    if (pci_isDevice(device) == false)
        return false;

    info->vendor            = pci_getVendorID(device);
    info->device            = pci_getDeviceID(device);
    info->subVendor         = pci_read16(device, 0x2CUL);
    info->subDevice         = pci_read16(device, 0x2EUL);
    info->isMultiFunction   = pci_isMultifunctionDevice(device);
    info->classCode         = pci_getClass(device);
    info->subClass          = pci_getSubClass(device);
    info->progIF            = pci_read8(device, 0x09UL);
    info->revision          = pci_read8(device, 0x08UL);
    info->headerType        = (pci_HeaderType) (pci_read8(device, 0x0EUL) & 0x7F);
    info->expansionRomPtr   = pci_read32(device, 0x30UL);

    DBG("VEN %04x DEV %04x CLASS %01x SUBCLASS %01x HDRTYPE %02x\n", info->vendor, info->device, info->classCode, info->subClass, info->headerType);

    if (info->headerType == 0) {
        for (i = 0UL; i < 6UL; i++) {
            info->bars[i].address = pci_read32(device, 0x10UL + i * 4UL);
            info->bars[i].type = (pci_BARType) (info->bars[i].address & 0x01);
            info->bars[i].size = 0UL;

            /* Mask info bits from BAR address depending on type */
            if (info->bars[i].type == PCI_BAR_MEMORY) {
                info->bars[i].address &= 0xFFFFFFF0UL;
                info->bars[i].size = pci_getBARSize(device, i);
            }

            if (info->bars[i].type == PCI_BAR_IO) {
                info->bars[i].address &= 0xFFFFFFFCUL;
            }

            if (info->bars[i].address > 0)
                DBG(" --> BAR[%lu] = %08lx TYPE %d SIZE %lu KB\n", i, info->bars[i].address,info->bars[i].type, info->bars[i].size / 1024UL);
        }
    }

    return (info->classCode < __CLASS_MAX__) && (info->headerType < __HEADERTYPE_MAX__);
}

bool pci_test(void) {
    u32 test = 0;

    /* Concept stolen from linux kernel :P */
    outp(0xCFB, 0x01);
    test = sys_inPortL(0xCF8);
    sys_outPortL(0xCF8, 0x80000000UL);
    test = sys_inPortL(0xCF8);

    if (test != 0x80000000UL) {
        DBG("ERROR while testing PCI configuration space access!\n");
        DBG("Expected 0x80000000, got 0x%08lx\n", test);
        return false;
    }

    return true;
}
