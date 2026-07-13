/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Z0008: zone_reset_while_append_inflight
 *
 * Submit a zone append, then immediately send a zone reset for the
 * same zone before the append completes. Tests device handling of
 * concurrent zone management operations.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_zone_reset_during_append(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << VIRTIO_BLK_F_ZONED)))
        return TEST_SKIP;

    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /* Use queue 0 for append, set up queue 1 for reset */
    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 1);

    /* Zone append on queue 0 */
    struct virtio_blk_outhdr *hdr0 = vv_alloc_pages(1);
    uint8_t *data0 = vv_alloc_pages(1);
    uint8_t *st0 = vv_alloc_pages(1);

    hdr0->type = VIRTIO_BLK_T_ZONE_APPEND;
    hdr0->ioprio = 0;
    hdr0->sector = 0; /* first zone */
    memset(data0, 0x42, 512);
    *st0 = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr0), sizeof(*hdr0),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data0), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st0), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* Zone reset on queue 1 */
    struct virtio_blk_outhdr *hdr1 = vv_alloc_pages(1);
    uint8_t *st1 = vv_alloc_pages(1);

    hdr1->type = VIRTIO_BLK_T_ZONE_RESET;
    hdr1->ioprio = 0;
    hdr1->sector = 0; /* same zone */
    *st1 = 0xFF;

    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(hdr1), sizeof(*hdr1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr2, 1, vv_virt_to_phys(st1), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    /* Kick both simultaneously */
    __sync_synchronize();
    uint16_t before0 = vr->used->idx;
    uint16_t before1 = vr2.used->idx;
    virtio_pci_kick(dev, 0);
    virtio_pci_kick(dev, 1);

    /* Wait for both (either may error, but device shouldn't crash) */
    int elapsed = 0;
    int done0 = 0, done1 = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (!done0 && vr->used->idx != before0)
            done0 = 1;
        if (!done1 && vr2.used->idx != before1)
            done1 = 1;
        if (done0 && done1)
            return TEST_PASS;
        elapsed += 10000;
    }

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST_REQUIRES(Z0008, VIRTIO_PCI_DEVICE_BLK, test_zone_reset_during_append,
              "Zone reset issued while zone append is in-flight",
              VIRTIO_SPEC_V1_2, "5.2.6.5",
              (1ULL << VIRTIO_BLK_F_ZONED), 2);
