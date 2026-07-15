/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0113: used ring entry id matches avail ring head descriptor.
 *
 * Spec 2.7.8: The device writes the head descriptor index into
 * the id field of each used ring entry. Submit a request starting
 * at descriptor 0, verify used->ring[0].id == 0.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_used_id(struct virtio_dev *dev, struct vring *vr)
{
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

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    uint32_t id = vr->used->ring[0].id;
    if (id != 0)
        TFAIL("used id %u, expected 0 (head descriptor)", id);

    return TEST_PASS;
}

REGISTER_TEST(T0113, VIRTIO_PCI_DEVICE_BLK, test_used_id,
              "Used ring entry id matches head descriptor index",
              VIRTIO_SPEC_V1_2, "2.7.8");
