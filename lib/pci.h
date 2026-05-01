/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_PCI_H
#define VV_PCI_H

#include <stddef.h>
#include <stdint.h>

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
