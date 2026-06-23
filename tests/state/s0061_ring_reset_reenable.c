/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0061: ring_reset_reenable
 *
 * Reset a queue via RING_RESET, then re-enable it with fresh
 * configuration (new addresses, possibly new size). Spec 2.2.1:
 * re-enabling follows the same process as initial queue discovery.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_ring_reset_reenable(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Check if RING_RESET is offered */
    cfg->device_feature_select = VIRTIO_F_RING_RESET / 32;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << (VIRTIO_F_RING_RESET % 32))))
        return TEST_SKIP;

    /* Reset queue 0 via the queue_reset register (spec 2.6.1) */
    if (virtio_pci_queue_reset(dev, 0) < 0)
        TFAIL("queue_enable not cleared after reset");

    if (cfg->queue_enable != 0)
        TFAIL("cfg->queue_enable != 0");

    /* Re-enable with fresh vring */
    struct vring vr2;
    vring_alloc(&vr2, 64);
    vring_attach(dev, &vr2, 0);

    /* Now submit I/O on the re-enabled queue */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr2, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&vr2, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    (void)vr;
    return vv_kick_and_wait(dev, &vr2, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0061, VIRTIO_PCI_DEVICE_BLK, test_ring_reset_reenable,
              "Re-enable queue after RING_RESET with fresh config",
              VIRTIO_SPEC_V1_3, "2.2.1");
