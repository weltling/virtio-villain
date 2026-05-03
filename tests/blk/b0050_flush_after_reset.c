/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0050: blk_flush_after_reset
 *
 * Reset the device, reinitialize, then immediately issue a FLUSH.
 * Tests device handling of flush on a freshly initialized queue
 * with no prior writes to persist.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_FLUSH 4

static test_result_t test_blk_flush_after_reset(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset device */
    virtio_pci_reset(dev);

    /* Reinitialize */
    cfg->device_status = 1 | 2; /* ACK + DRIVER */
    __sync_synchronize();
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat = cfg->device_feature;
    cfg->driver_feature_select = 0;
    cfg->driver_feature = feat;
    __sync_synchronize();
    cfg->device_status = 1 | 2 | 8; /* FEATURES_OK */
    __sync_synchronize();
    usleep(1000);
    if (!(cfg->device_status & 8))
        TREJECT("!(cfg->device_status & 8)");
    cfg->device_status = 1 | 2 | 8 | 4; /* DRIVER_OK */
    __sync_synchronize();

    /* Set up queue */
    struct vring tvr;
    vring_alloc(&tvr, 16);
    vring_attach(dev, &tvr, 0);

    /* Issue FLUSH immediately */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_FLUSH;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&tvr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&tvr, 1, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&tvr, 0, 0);
    vring_raw_set_avail_idx(&tvr, 1);

    return vv_kick_and_wait(dev, &tvr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0050, VIRTIO_PCI_DEVICE_BLK, test_blk_flush_after_reset,
              "FLUSH immediately after device reset and reinit",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
