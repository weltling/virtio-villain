/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0044: blk_get_id_nonzero_queue
 *
 * Submit a GET_ID request via a non-zero queue index (multiqueue).
 * Some VMMs only handle GET_ID on queue 0. If multiqueue is
 * advertised, the device should handle GET_ID on any request queue.
 */
#include "lib/virtio_test.h"

static test_result_t test_blk_get_id_nonzero_queue(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    (void)vr; /* We use queue 1 instead */

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;
    if (nq < 2)
        return TEST_SKIP;

    /* Set up queue 1 */
    struct vring vr1;
    vring_alloc(&vr1, 16);
    vring_attach(dev, &vr1, 1);

    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_GET_ID, 0);
    uint8_t *id_buf = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(&vr1, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(&vr1, 1, id_buf, 20, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(&vr1, 2, status, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&vr1, 0, 0);
    vring_raw_set_avail_idx(&vr1, 1);

    return vv_kick_and_wait(dev, &vr1, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0044, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id_nonzero_queue,
              "GET_ID on non-zero queue (multiqueue)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
