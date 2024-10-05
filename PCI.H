/*  LIB866D
    PCI device configuration functions

    (C) 2024 E. Voirin (oerg866)
*/

#ifndef _PCI_H_
#define _PCI_H_

#include "types.h"

#define PCI_BUS_MAX     255     /* Maximum of 256 PCI buses per machine */
#define PCI_SLOT_MAX    31      /* Maximum of 32 PCI Slots per bus */
#define PCI_FUNC_MAX    7       /* Maximum of 8 Functions per PCI device */
#define PCI_BARS_MAX    5       /* Maximum of 6 BARs per PCI device */

/* PCI header types */
typedef enum {
    PCI_ENDPOINT = 0,
    PCI_PCI2PCI_BRIDGE = 1,
    PCI_PCI2CARDBUS_BRIDGE = 2,
    __HEADERTYPE_MAX__
} pci_HeaderType;

/* PCI Base Address Register Types */
typedef enum {
    PCI_BAR_MEMORY = 0x00,
    PCI_BAR_IO     = 0x01,
    __PCI_BAR_MAX__
} pci_BARType;

/* PCI Device Classes */
typedef enum {
    CLASS_UNCLASSIFIED             = 0x00,  /* Devices that do not fit any class                    */
    CLASS_MASS_STORAGE             = 0x01,  /* Mass storage controllers (e.g., SCSI, IDE)           */
    CLASS_NETWORK                  = 0x02,  /* Network controllers (e.g., Ethernet)                 */
    CLASS_DISPLAY                  = 0x03,  /* Display controllers (e.g., VGA)                      */
    CLASS_MULTIMEDIA               = 0x04,  /* Multimedia controllers (e.g., audio, video)          */
    CLASS_MEMORY                   = 0x05,  /* Memory controllers (e.g., DRAM)                      */
    CLASS_BRIDGE                   = 0x06,  /* Bridge devices (e.g., PCI-to-PCI bridges)            */
    CLASS_SIMPLE_COMMUNICATION     = 0x07,  /* Simple communication controllers (e.g., UART, modem) */
    CLASS_BASE_SYSTEM_PERIPHERAL   = 0x08,  /* Base system peripherals (e.g., DMA, PIC)             */
    CLASS_INPUT_DEVICES            = 0x09,  /* Input devices (e.g., keyboard, mouse)                */
    CLASS_DOCKING_STATIONS         = 0x0A,  /* Docking stations                                     */
    CLASS_PROCESSORS               = 0x0B,  /* Processors (e.g., i386)                              */
    CLASS_SERIAL_BUS_CTRL          = 0x0C,  /* Serial bus controllers (e.g., USB, FireWire)         */
    CLASS_WIRELESS_CTRL            = 0x0D,  /* Wireless controllers (e.g., Bluetooth, Wi-Fi)        */
    CLASS_INTELLIGENT_CTRL         = 0x0E,  /* Intelligent I/O controllers                          */
    CLASS_SATELLITE_COMMUNICATION  = 0x0F,  /* Satellite communication controllers                  */
    CLASS_ENCRYPTION_CTRL          = 0x10,  /* Encryption/Decryption controllers                    */
    CLASS_SIGNAL_PROCESSING_CTRL   = 0x11,  /* Data acquisition and signal processing controllers   */
    __CLASS_MAX__                  = 0x12,  /*                                                      */
    CLASS_RESERVED                 = 0xFF   /* Reserved class (no device should use this)           */
} pci_Class;

#pragma pack(1)
typedef struct {
    u8 bus;
    u8 slot;
    u8 func;
    u8 dummy;
} pci_Device;

typedef struct {
    u16             vendor;
    u16             device;
    u16             subVendor;
    u16             subDevice;
    bool            isMultiFunction;
    pci_Class       classCode;
    u8              subClass;
    u8              progIF;
    u8              revision;
    pci_HeaderType  headerType;
    u32             expansionRomPtr;
    struct {
        u32         address;
        pci_BARType type;
        u32         size;
    } bars[6];
} pci_DeviceInfo;
#pragma pack()


/*  Print debug info about a given device */
void pci_debugInfo(pci_Device device);

/*  Reads a 32-bit word from the given pci_Device's configuration space */
u32 pci_read32(pci_Device device, u32 offset);
/*  Reads a 16-bit word from the given pci_Device's configuration space */
u16 pci_read16(pci_Device device, u32 offset);
/*  Reads a 8-bit value from the given pci_Device's configuration space */
u8  pci_read8 (pci_Device device, u32 offset);

/*  Reads <count> bytes at <offset> from <device>'s cfg space into <buffer> */
void pci_readBytes(pci_Device device, u8 *buffer, u32 offset, u32 count);

/*  Writes a 32-bit word to the given pci_Device's configuration space */
void pci_write32(pci_Device device, u32 offset, u32 value);
/*  Writes a 16-bit word to the given pci_Device's configuration space */
void pci_write16(pci_Device device, u32 offset, u16 value);
/*  Writes a 8-bit value to the given pci_Device's configuration space */
void pci_write8 (pci_Device device, u32 offset, u8 value);

/*  Get the PCI Vendor ID for the given pci_Device */
u16 pci_getVendorID(pci_Device device);
/*  Get the PCI Device ID for the given pci_Device */
u16 pci_getDeviceID(pci_Device device);

#if 0
/*  Get the PCI Header Type for the given pci_Device */
pci_HeaderType pci_getHeaderType(pci_Device device);
/*  Get the PCI Device Class for the given pci_Device */
pci_Class pci_getClass(pci_Device device);
/*  Get the PCI Device SubClass for the given pci_Device */
pci_Class pci_getSubClass(pci_Device device);
#endif

/*  Tries to find a PCI device with the given vendor/device ID.
    If successful, returns true and populates <device>, else false. */
bool pci_findDevByID(u16 ven, u16 dev, pci_Device *device);

/*  Get the next PCI device *after* <device>.
    Call with NULL pointer to get the first device.
    Returns NULL if no more devices are found.

    Can be used to iterate through all devices in the system. */
pci_Device *pci_getNextDevice(pci_Device *device);

/*  Populates a pci_DeviceInfo structure from a pci_Device, which contains
    a lot of useful information */
bool pci_populateDeviceInfo(pci_DeviceInfo *info, pci_Device device);

/*  Test if current machine's PCI bus can be read/written by this application.
    on the current system.
    Returns 1 if yes, 0 if not. */
bool pci_test();

#endif
