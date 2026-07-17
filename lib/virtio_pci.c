/* SPDX-License-Identifier: Apache-2.0 */
#include "virtio_pci.h"
#include "pci.h"
#include "util.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

struct virtio_caps {
    uint8_t  common_bar;
    uint32_t common_offset;
    uint32_t common_length;
    uint8_t  notify_bar;
    uint32_t notify_offset;
    uint32_t notify_length;
    uint32_t notify_off_multiplier;
    uint8_t  device_bar;
    uint32_t device_offset;
    uint32_t device_length;
    uint8_t  isr_bar;
    uint32_t isr_offset;
    uint32_t isr_length;
    uint8_t  pci_cfg_cap_offset; /* for VIRTIO_PCI_CAP_PCI_CFG */
};

static int parse_caps(const char *slot, struct virtio_caps *caps)
{
    int fd = pci_cfg_open(slot);
    if (fd < 0)
        return -1;

    uint8_t cap_ptr = pci_cfg_read8(fd, 0x34);
    memset(caps, 0, sizeof(*caps));

    while (cap_ptr) {
        uint8_t cap_id = pci_cfg_read8(fd, cap_ptr);
        uint8_t cap_next = pci_cfg_read8(fd, cap_ptr + 1);

        /* Vendor specific capability (0x09) for virtio */
        if (cap_id == 0x09) {
            uint8_t cfg_type = pci_cfg_read8(fd, cap_ptr + 3);
            uint8_t bar = pci_cfg_read8(fd, cap_ptr + 4);
            uint32_t offset = pci_cfg_read32(fd, cap_ptr + 8);
            uint32_t length = pci_cfg_read32(fd, cap_ptr + 12);

            if (cfg_type == VIRTIO_PCI_CAP_COMMON_CFG) {
                caps->common_bar = bar;
                caps->common_offset = offset;
                caps->common_length = length;
            } else if (cfg_type == VIRTIO_PCI_CAP_NOTIFY_CFG) {
                caps->notify_bar = bar;
                caps->notify_offset = offset;
                caps->notify_length = length;
                caps->notify_off_multiplier = pci_cfg_read32(fd, cap_ptr + 16);
            } else if (cfg_type == VIRTIO_PCI_CAP_DEVICE_CFG) {
                caps->device_bar = bar;
                caps->device_offset = offset;
                caps->device_length = length;
            } else if (cfg_type == VIRTIO_PCI_CAP_ISR_CFG) {
                caps->isr_bar = bar;
                caps->isr_offset = offset;
                caps->isr_length = length;
            } else if (cfg_type == VIRTIO_PCI_CAP_PCI_CFG) {
                caps->pci_cfg_cap_offset = cap_ptr;
            }
        }
        cap_ptr = cap_next;
    }
    close(fd);
    return (caps->common_length > 0) ? 0 : -1;
}

int virtio_pci_attach(uint16_t device_id, struct virtio_dev *dev)
{
    dev->device_id = device_id;
    pci_enable(dev->slot);

    struct virtio_caps caps;
    if (parse_caps(dev->slot, &caps) < 0)
        return -1;

    dev->bar = pci_map_bar(dev->slot, caps.common_bar);
    if (!dev->bar)
        return -1;
    dev->bar_phys = pci_bar_phys(dev->slot, caps.common_bar);

    dev->common = (volatile struct virtio_pci_common_cfg *)
        ((char *)dev->bar + caps.common_offset);
    dev->common_phys = dev->bar_phys + caps.common_offset;
    dev->notify_base = (volatile uint16_t *)
        ((char *)dev->bar + caps.notify_offset);
    dev->notify_off_multiplier = caps.notify_off_multiplier;
    dev->notify_length = caps.notify_length;
    dev->common_length = caps.common_length;

    if (caps.device_length > 0) {
        dev->device_cfg = (volatile void *)
            ((char *)dev->bar + caps.device_offset);
        dev->device_cfg_length = caps.device_length;
    } else {
        dev->device_cfg = NULL;
        dev->device_cfg_length = 0;
    }

    if (caps.isr_length > 0) {
        dev->isr = (volatile uint8_t *)
            ((char *)dev->bar + caps.isr_offset);
        dev->isr_length = caps.isr_length;
    } else {
        dev->isr = NULL;
        dev->isr_length = 0;
    }

    dev->pci_cfg_cap_offset = caps.pci_cfg_cap_offset;

    return 0;
}

int virtio_pci_find(uint16_t device_id, struct virtio_dev *dev)
{
    if (pci_find_device(VIRTIO_PCI_VENDOR, device_id, dev->slot, sizeof(dev->slot)) < 0)
        return -1;
    return virtio_pci_attach(device_id, dev);
}

int virtio_pci_init_features(struct virtio_dev *dev, uint64_t wanted)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset. Per spec 4.1.4.3.1, the driver MUST re-read device_status
     * to ensure the reset has completed before proceeding. The device
     * may take an arbitrary amount of wall time to acknowledge the
     * reset under host CPU contention. */
    cfg->device_status = 0;
    __sync_synchronize();
    for (int i = 0; i < 1000; i++) {
        __sync_synchronize();
        if (cfg->device_status == 0)
            break;
        usleep(1000);
    }
    if (cfg->device_status != 0)
        return -1;

    /* Acknowledge */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();

    /* Driver */
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /*
     * Negotiate the requested feature bits, restricted to what the
     * device actually offers. A test that needs a feature the device
     * does not offer will observe its absence and skip.
     */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t lo = cfg->device_feature;
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t hi = cfg->device_feature;
    uint64_t offered = ((uint64_t)hi << 32) | lo;
    uint64_t neg = wanted & offered;

    cfg->driver_feature_select = 0;
    cfg->driver_feature = (uint32_t)(neg & 0xFFFFFFFFU);
    cfg->driver_feature_select = 1;
    cfg->driver_feature = (uint32_t)(neg >> 32);
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    /* Per spec 2.2.2 / 2.1.2, the FEATURES_OK acceptance is signalled
     * by the device leaving the bit set on read back. Under host CPU
     * contention the VMM may not have processed the write before the
     * guest reads. Poll for acceptance instead of failing on the very
     * first read. */
    for (int i = 0; i < 1000; i++) {
        __sync_synchronize();
        if (cfg->device_status & VIRTIO_STATUS_FEATURES_OK)
            return 0;
        usleep(1000);
    }
    return -1;
}

int virtio_pci_init(struct virtio_dev *dev)
{
    return virtio_pci_init_features(dev, 0);
}

void virtio_pci_reset(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_status = 0;
    __sync_synchronize();
    /* Wait for the device to confirm reset (spec 4.1.4.3.1). */
    for (int i = 0; i < 1000; i++) {
        __sync_synchronize();
        if (cfg->device_status == 0)
            return;
        usleep(1000);
    }
}

int virtio_pci_queue_reset(struct virtio_dev *dev, uint16_t queue)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->queue_select = queue;
    __sync_synchronize();
    cfg->queue_reset = 1;
    __sync_synchronize();

    /*
     * Spec 2.6.1: the device clears queue_enable once the reset has
     * completed. A synchronous device does this within the same MMIO
     * write; poll briefly to tolerate an asynchronous one.
     */
    for (int i = 0; i < 1000; i++) {
        __sync_synchronize();
        if (cfg->queue_enable == 0)
            return 0;
        usleep(1000);
    }
    return -1;
}

void virtio_pci_kick(struct virtio_dev *dev, uint16_t queue)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->queue_select = queue;
    __sync_synchronize();
    uint16_t off = cfg->queue_notify_off;
    volatile uint16_t *addr = (volatile uint16_t *)
        ((char *)dev->notify_base + off * dev->notify_off_multiplier);
    *addr = queue;
    __sync_synchronize();
}
