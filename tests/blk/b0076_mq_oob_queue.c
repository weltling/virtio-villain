/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0076: MQ request on queue index > num_queues
 *
 * Spec 5.2.5: "N=1 if VIRTIO_BLK_F_MQ is not negotiated, otherwise
 * N is set by num_queues."
 *
 * Try to use a request queue beyond the device's num_queues. If
 * the device only has 1 request queue, attempt queue index 1.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_mq_oob_queue(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;

    /* Use queue index = nq (one beyond valid range) */
    uint16_t oob_q = nq;

    struct vring oob_vr;
    vring_alloc(&oob_vr, 16);
    vring_attach(dev, &oob_vr, oob_q);

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(&oob_vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(&oob_vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(&oob_vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&oob_vr, 0, 0);
    vring_raw_set_avail_idx(&oob_vr, 1);

    return vv_kick_and_wait(dev, &oob_vr, oob_q, VV_TIMEOUT_MS);
}

REGISTER_TEST_DEV(B0076, VIRTIO_PCI_DEVICE_BLK, test_blk_mq_oob_queue,
                  "Request on queue index beyond num_queues",
                  VIRTIO_SPEC_V1_2, "5.2.5");
