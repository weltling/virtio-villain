/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0057: avail_idx_decrement_by_one
 *
 * Decrement avail.idx by exactly 1 after a valid submission and kick.
 * Unlike T24 which makes a large backwards jump, this is a subtle
 * off-by-one that some VMMs may not detect because (idx - 1) wraps
 * to look like one fewer pending entry rather than 65535.
 *
 * Spec 2.7.7.1: driver MUST NOT decrement the available idx.
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

#define VIRTIO_BLK_T_IN 0

static test_result_t test_avail_idx_decrement_by_one(struct virtio_dev *dev,
                                                     struct vring *vr)
{
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

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1, VRING_DESC_F_WRITE, 0);

    /* Submit 2 entries so VMM records avail_idx=2 */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);
    virtio_pci_kick(dev, 0);
    usleep(200000);
    __sync_synchronize();

    /* Now decrement by exactly 1: set avail_idx=1 */
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0057, VIRTIO_PCI_DEVICE_BLK, test_avail_idx_decrement_by_one,
              "Decrement avail idx by exactly 1 (subtle underflow)",
              VIRTIO_SPEC_V1_2, "2.7.7.1");
