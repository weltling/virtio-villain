/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0017: state_enable_queue_twice
 *
 * Enable the same queue twice without an intervening device reset.
 * The spec requires queue_enable transitions from 0 to 1 only once.
 * Writing 1 again after it's already enabled is a protocol violation.
 */
#include "lib/virtio_test.h"

static test_result_t test_enable_queue_twice(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /*
     * The queue is already enabled by the harness (DRIVER_OK was set).
     * Select queue 0 and write queue_enable=1 again.
     */
    cfg->queue_select = 0;
    __sync_synchronize();

    /* Queue should already be enabled */
    if (!cfg->queue_enable)
        return TEST_SKIP;

    /* Write enable again - this is a violation */
    cfg->queue_enable = 1;
    __sync_synchronize();
    usleep(10000);

    /* Try to do I/O - device should still work or reject gracefully */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(S0017, VIRTIO_PCI_DEVICE_BLK, test_enable_queue_twice,
              "Enable same queue twice without reset",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");
