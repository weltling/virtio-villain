/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0086: Set descriptor table address to a non-16-byte aligned address.
 *
 * Spec 2.7.5.1 requires the descriptor table to be 16-byte aligned.
 * We configure queue_desc with a misaligned physical address, then
 * submit a request to see if the device rejects or crashes.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_desc_table_unaligned(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset */
    virtio_pci_reset(dev);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(5000);

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    /* Allocate pages for queue structures */
    void *page = vv_alloc_pages(4);
    uint64_t phys = vv_virt_to_phys(page);

    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_size = 16;

    /* Set desc table to misaligned address (offset +7, not 16-byte aligned) */
    cfg->queue_desc = phys + 7;
    cfg->queue_avail = phys + 4096;
    cfg->queue_used = phys + 8192;
    cfg->queue_enable = 1;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Set up a simple request at the misaligned desc table */
    struct vring misaligned_vr;
    misaligned_vr.desc = (struct vring_desc *)((char *)page + 7);
    misaligned_vr.avail = (struct vring_avail *)((char *)page + 4096);
    misaligned_vr.used = (struct vring_used *)((char *)page + 8192);
    misaligned_vr.size = 16;
    misaligned_vr.queue = 0;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(&misaligned_vr, 0, vv_virt_to_phys(hdr),
                       sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&misaligned_vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&misaligned_vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&misaligned_vr, 0, 0);
    vring_raw_set_avail_idx(&misaligned_vr, 1);

    return vv_kick_and_wait(dev, &misaligned_vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0086, VIRTIO_PCI_DEVICE_BLK, test_desc_table_unaligned,
              "Descriptor table at non-16-byte aligned address",
              VIRTIO_SPEC_V1_2, "2.7.5.1");
