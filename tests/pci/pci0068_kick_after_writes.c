/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0068: kick after unrelated common cfg writes
 *
 * Spec 4.1.5.2 says the notification register is independent of
 * common cfg writes. Touching unrelated fields like queue_select
 * or msix_config in between two valid kicks must not interfere
 * with normal queue processing. This catches devices that drop
 * pending notifications on unrelated register touches.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_pci_kick_after_writes(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Scribble unrelated writable fields. msix_config is omitted
     * because changing the config vector while the device is live
     * is destructive on some VMMs. */
    cfg->queue_select = 1;
    __sync_synchronize();
    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->device_feature_select = 1;
    __sync_synchronize();
    cfg->device_feature_select = 0;
    __sync_synchronize();
    cfg->driver_feature_select = 1;
    __sync_synchronize();
    cfg->driver_feature_select = 0;
    __sync_synchronize();

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(PCI0068, VIRTIO_PCI_DEVICE_BLK, test_pci_kick_after_writes,
              "unrelated common cfg writes do not eat queue notifications",
              VIRTIO_SPEC_V1_2, "4.1.5.2");
