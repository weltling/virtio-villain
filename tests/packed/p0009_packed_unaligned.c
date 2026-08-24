/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0009: packed_unaligned_address
 *
 * Set the packed virtqueue descriptor ring address to a non-aligned
 * value. The packed descriptor ring should be aligned to 16 bytes
 * (descriptor size).
 * Spec 2.8.11: descriptor table MUST be aligned.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_packed_unaligned(struct virtio_dev *dev,
                                           struct vring_packed *vr)
{
    (void)vr;

    /* Reset to reconfigure queue with misaligned address */
    virtio_pci_reset(dev);

    if (virtio_pci_init_packed(dev) < 0)
        return TEST_SKIP;

    dev->common->queue_select = 0;
    __sync_synchronize();
    dev->common->queue_size = 16;

    /* Allocate a page and offset by 3 bytes for misalignment */
    uint8_t *page = vv_alloc_pages(2);
    uint64_t page_phys = vv_virt_to_phys(page);
    uint64_t misaligned_phys = page_phys + 3;

    virtio_store64(&dev->common->queue_desc, misaligned_phys);
    virtio_store64(&dev->common->queue_avail, page_phys + PAGE_SIZE);
    virtio_store64(&dev->common->queue_used, page_phys + PAGE_SIZE + 64);
    dev->common->queue_msix_vector = 0xffff;
    dev->common->queue_enable = 1;
    __sync_synchronize();

    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    virtio_pci_kick(dev, 0);
    usleep(200000);

    /* Reset and verify survival */
    virtio_pci_reset(dev);

    if (virtio_pci_init_packed(dev) < 0)
        return TEST_SKIP;

    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        vv_log("unaligned packed queue address made device unresponsive");
        TFAIL("!(dev->common->device_status & VIRTIO_STATUS_FEATURES_OK)");
    }

    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0009, VIRTIO_PCI_DEVICE_BLK, test_packed_unaligned,
                     "Unaligned packed virtqueue descriptor address",
                     VIRTIO_SPEC_V1_2, "2.8.11");
