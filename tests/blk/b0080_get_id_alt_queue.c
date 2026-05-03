/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0080: GET_ID request with header type on a different queue
 *
 * Spec 5.2.6: GET_ID returns 20-byte serial. Submit GET_ID on
 * queue 1 (if MQ available) to test device handles the request
 * type regardless of which request queue processes it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_GET_ID 8

static test_result_t test_blk_get_id_alt_queue(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;
    if (nq < 2)
        return TEST_SKIP;

    struct vring q1vr;
    vring_alloc(&q1vr, 16);
    vring_attach(dev, &q1vr, 1);

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_GET_ID;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0, 20);
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(&q1vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&q1vr, 1, data_phys, 20,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&q1vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&q1vr, 0, 0);
    vring_raw_set_avail_idx(&q1vr, 1);

    return vv_kick_and_wait(dev, &q1vr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0080, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id_alt_queue,
              "GET_ID request on alternate queue (queue 1)",
              VIRTIO_SPEC_V1_2, "5.2.6");
