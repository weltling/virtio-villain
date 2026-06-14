/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0065: ring_reset_other_queues_active
 *
 * Reset queue 0 while queue 1 remains active and processes I/O.
 * Spec 2.2.1: resetting one queue must not affect other queues.
 * Uses a multiqueue blk device (num_queues > 1).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_ring_reset_other_active(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Check for RING_RESET */
    cfg->device_feature_select = VIRTIO_F_RING_RESET / 32;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << (VIRTIO_F_RING_RESET % 32))))
        return TEST_SKIP;

    /* Need at least 2 queues */
    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /* Set up queue 1 */
    struct vring vr1;
    vring_alloc(&vr1, 64);
    vring_attach(dev, &vr1, 1);

    /* Reset queue 0 */
    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_enable = 0;
    __sync_synchronize();
    usleep(50000);

    /* Submit I/O on queue 1 which should still work */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&vr1, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr1, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&vr1, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&vr1, 0, 0);
    vring_raw_set_avail_idx(&vr1, 1);

    (void)vr;
    return vv_kick_and_wait(dev, &vr1, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0065, VIRTIO_PCI_DEVICE_BLK, test_ring_reset_other_active,
              "Reset queue 0 while queue 1 remains active",
              VIRTIO_SPEC_V1_3, "2.2.1");
