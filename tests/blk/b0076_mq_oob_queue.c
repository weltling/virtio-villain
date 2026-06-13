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
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_blk_mq_oob_queue(struct virtio_dev *dev,
                                           struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;

    /* Use queue index = nq (one beyond valid range) */
    uint16_t oob_q = nq;

    struct vring oob_vr;
    vring_alloc(&oob_vr, 16);
    vring_attach(dev, &oob_vr, oob_q);

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(&oob_vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&oob_vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&oob_vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&oob_vr, 0, 0);
    vring_raw_set_avail_idx(&oob_vr, 1);

    return vv_kick_and_wait(dev, &oob_vr, oob_q, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0076, VIRTIO_PCI_DEVICE_BLK, test_blk_mq_oob_queue,
              "Request on queue index beyond num_queues",
              VIRTIO_SPEC_V1_2, "5.2.5");
