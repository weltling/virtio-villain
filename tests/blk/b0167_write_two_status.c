/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0167: WRITE chain with two writable status descriptors.
 *
 * Spec 5.2.6: A blk request chain ends with exactly one writable
 * status byte. Submit a WRITE with a header, data, and two
 * writable status descriptors trailing instead of one. The
 * device must reject the malformed chain rather than writing
 * the status byte twice or treating the extra writable as
 * additional payload.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_OUT 1

static test_result_t test_blk_two_status(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data  = vv_alloc_pages(1);
    uint8_t *st1   = vv_alloc_pages(1);
    uint8_t *st2   = vv_alloc_pages(1);

    hdr->type   = VIRTIO_BLK_T_OUT;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0xAB, 512);
    *st1 = 0xFF;
    *st2 = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st1), 1,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(st2), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0167, VIRTIO_PCI_DEVICE_BLK, test_blk_two_status,
              "WRITE chain with two trailing status descriptors",
              VIRTIO_SPEC_V1_2, "5.2.6");
