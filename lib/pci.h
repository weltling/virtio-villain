/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_PCI_H
#define VV_PCI_H

#include <stddef.h>
#include <stdint.h>

/* PCI config space register offsets */
#define PCI_STATUS         0x06
#define PCI_CAP_PTR        0x34

/* Offsets within a PCI capability structure */
#define PCI_CAP_LIST_ID    0
#define PCI_CAP_LIST_NEXT  1

/* PCI capability IDs */
#define PCI_CAP_ID_MSI     0x05
#define PCI_CAP_ID_VNDR    0x09
#define PCI_CAP_ID_MSIX    0x11

/* MSI capability message control register bits */
#define MSI_CTRL_ENABLE    0x0001  /* MSI enable */
#define MSI_CTRL_MMC_MASK  0x000E  /* Multiple message capable */
#define MSI_CTRL_MME_MASK  0x0070  /* Multiple message enable */
#define MSI_CTRL_MME_SHIFT 4
#define MSI_CTRL_64BIT     0x0080  /* 64 bit address capable */
#define MSI_CTRL_PVM       0x0100  /* Per vector masking */

/* One entry in the MSI-X table (PCI 3.0 section 6.8.2). */
struct msix_table_entry {
    uint32_t msg_addr_lo;
    uint32_t msg_addr_hi;
    uint32_t msg_data;
    uint32_t vector_ctrl;   /* bit 0 = mask */
} __attribute__((packed));

/*
 * PCI config space access via sysfs.
 * Operates on /sys/bus/pci/devices/<slot>/config and resource files.
 */

/* Find a PCI device by vendor/device ID. Returns 0 on success. */
int pci_find_device(uint16_t vendor, uint16_t device, char *slot, size_t len);

/* Enable a PCI device (write 1 to its "enable" sysfs file). */
void pci_enable(const char *slot);

/* Read from PCI config space at the given offset. */
uint8_t  pci_cfg_read8(int fd, uint32_t offset);
uint16_t pci_cfg_read16(int fd, uint32_t offset);
uint32_t pci_cfg_read32(int fd, uint32_t offset);

/* Write to PCI config space at the given offset. */
void pci_cfg_write8(int fd, uint32_t offset, uint8_t val);
void pci_cfg_write32(int fd, uint32_t offset, uint32_t val);

/* Open the config space file for a PCI slot. Caller must close(). */
int pci_cfg_open(const char *slot);

/* Map a PCI BAR resource file. Returns mapped address or NULL on failure. */
volatile void *pci_map_bar(const char *slot, int bar);

/* Return the guest physical base address of a PCI BAR, read from the
 * sysfs "resource" file. Returns 0 if the BAR is unimplemented or the
 * file cannot be parsed. MMIO mappings returned by pci_map_bar do not
 * appear as present pages in /proc/self/pagemap, so callers that need
 * the bus address of an MMIO region must use this helper. */
uint64_t pci_bar_phys(const char *slot, int bar);

#endif /* VV_PCI_H */
