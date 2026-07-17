/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_VIRTIO_PCI_H
#define VV_VIRTIO_PCI_H

#include <stdint.h>

/* Virtio PCI capability types (spec 4.1.4) */
#define VIRTIO_PCI_CAP_COMMON_CFG    1
#define VIRTIO_PCI_CAP_NOTIFY_CFG    2
#define VIRTIO_PCI_CAP_ISR_CFG       3
#define VIRTIO_PCI_CAP_DEVICE_CFG    4
#define VIRTIO_PCI_CAP_PCI_CFG       5
#define VIRTIO_PCI_CAP_SHARED_MEM    8
#define VIRTIO_PCI_CAP_VENDOR_CFG    9

/* Used in the MSI-X vector config fields to mean "no vector" (spec 4.1.5.1.2) */
#define VIRTIO_MSI_NO_VECTOR  0xFFFF

/* Virtio device status bits (spec 2.1) */
#define VIRTIO_STATUS_ACKNOWLEDGE  1
#define VIRTIO_STATUS_DRIVER       2
#define VIRTIO_STATUS_DRIVER_OK    4
#define VIRTIO_STATUS_FEATURES_OK  8
#define VIRTIO_STATUS_SUSPEND      16  /* proposal, not in a released spec */
#define VIRTIO_STATUS_NEEDS_RESET  64
#define VIRTIO_STATUS_FAILED       128

/* Virtio PCI vendor ID */
#define VIRTIO_PCI_VENDOR  0x1af4

/* Modern virtio PCI device IDs (transitional base + 0x40) */
#define VIRTIO_PCI_DEVICE_NET      0x1041
#define VIRTIO_PCI_DEVICE_BLK      0x1042
#define VIRTIO_PCI_DEVICE_CONSOLE  0x1043
#define VIRTIO_PCI_DEVICE_RNG      0x1044
#define VIRTIO_PCI_DEVICE_BALLOON  0x1045
#define VIRTIO_PCI_DEVICE_VSOCK    0x1053
#define VIRTIO_PCI_DEVICE_IOMMU    0x1057
#define VIRTIO_PCI_DEVICE_MEM      0x1058
#define VIRTIO_PCI_DEVICE_PMEM     0x105b
#define VIRTIO_PCI_DEVICE_WATCHDOG 0x1063
#define VIRTIO_PCI_DEVICE_RTC      0x1051
#define VIRTIO_PCI_DEVICE_FS       0x105a

/*
 * Virtio PCI modern common configuration layout (spec 4.1.4.3).
 */
struct virtio_pci_common_cfg {
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t driver_feature_select;
    uint32_t driver_feature;
    uint16_t msix_config;
    uint16_t num_queues;
    uint8_t  device_status;
    uint8_t  config_generation;
    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint64_t queue_desc;
    uint64_t queue_avail;
    uint64_t queue_used;
    uint16_t queue_notify_data;
    uint16_t queue_reset;
} __attribute__((packed));

/* ISR status register bits (spec 4.1.4.5) */
#define VIRTIO_PCI_ISR_QUEUE   1
#define VIRTIO_PCI_ISR_CONFIG  2

/*
 * Parsed virtio PCI device state.
 */
struct virtio_dev {
    volatile void *bar;                        /* primary BAR mapping */
    volatile struct virtio_pci_common_cfg *common; /* common cfg struct */
    volatile uint16_t *notify_base;            /* notification area base */
    volatile void *device_cfg;                 /* device-specific config */
    volatile uint8_t *isr;                     /* ISR status register */
    uint64_t bar_phys;                         /* primary BAR guest physical base */
    uint64_t common_phys;                      /* guest physical addr of common cfg */
    uint32_t notify_off_multiplier;            /* notify offset multiplier */
    uint32_t device_cfg_length;                /* device config region size */
    uint32_t notify_length;                    /* notification region size */
    uint32_t common_length;                    /* common cfg region size */
    uint32_t isr_length;                       /* ISR region size */
    uint16_t device_id;                        /* virtio device type */
    uint8_t  pci_cfg_cap_offset;               /* PCI cfg access cap offset */
    char slot[256];                            /* PCI BDF slot string */
};

/*
 * Find and initialize a virtio PCI device by device ID.
 * Parses capabilities, maps BARs, returns 0 on success.
 */
int virtio_pci_find(uint16_t device_id, struct virtio_dev *dev);

/*
 * Same as virtio_pci_find, but the caller has already filled
 * dev->slot with the absolute path under /sys/bus/pci/devices.
 * Useful when the caller needs to disambiguate between several
 * functions of the same device class, for example after PCI hot
 * add when more than one virtio-blk function exists.
 */
int virtio_pci_attach(uint16_t device_id, struct virtio_dev *dev);

/*
 * Perform virtio device initialization sequence up to DRIVER_OK.
 * Negotiates zero features for simplicity.
 */
int virtio_pci_init(struct virtio_dev *dev);

/*
 * Like virtio_pci_init, but negotiates the requested feature bits
 * (restricted to those the device offers) instead of zero.
 */
int virtio_pci_init_features(struct virtio_dev *dev, uint64_t wanted);

/*
 * Return non-zero if the device offers the given feature bit. Handles
 * the low and high feature words via device_feature_select.
 */
static inline int virtio_pci_feature_offered(struct virtio_dev *dev,
                                             unsigned bit)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = bit >> 5;
    __sync_synchronize();
    return (cfg->device_feature >> (bit & 31)) & 1u;
}

/* Reset device (set status to 0). */
void virtio_pci_reset(struct virtio_dev *dev);

/*
 * Reset a single virtqueue via the queue_reset register (spec 2.6.1,
 * requires the device to offer VIRTIO_F_RING_RESET). Selects the
 * queue, writes queue_reset = 1, then polls until the device clears
 * queue_enable. Returns 0 once the queue is disabled, -1 on timeout.
 */
int virtio_pci_queue_reset(struct virtio_dev *dev, uint16_t queue);

/* Send a queue notification (kick). */
void virtio_pci_kick(struct virtio_dev *dev, uint16_t queue);

#endif /* VV_VIRTIO_PCI_H */
