/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0185: flush on a fresh device before any writes.
 *
 * Spec 5.2.6.2: FLUSH makes all prior writes stable. If no writes
 * have occurred, the flush should succeed trivially. The device
 * must not fail or hang on an empty flush.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_flush_empty(struct virtio_dev *dev,
                                          struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_FLUSH)))
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_FLUSH;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    if (*st != VIRTIO_BLK_S_OK)
        TFAIL("flush status %u on fresh device", *st);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0185, VIRTIO_PCI_DEVICE_BLK, test_blk_flush_empty,
              "Flush on fresh device before any writes",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_FLUSH), 0);
