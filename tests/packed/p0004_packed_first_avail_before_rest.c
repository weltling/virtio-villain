/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0004: packed_first_avail_before_rest
 *
 * Make the first descriptor in a chain available (set AVAIL bit)
 * before the subsequent descriptors are fully written. The device
 * may read garbage from not-yet-written descriptors.
 * Spec 2.8.17: driver MUST make the first descriptor available only
 * after all subsequent descriptors have been written.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_packed_first_avail_before_rest(struct virtio_dev *dev,
                                                         struct vring_packed *vr)
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

    /*
     * Make desc[0] available FIRST (with AVAIL bit set),
     * but leave desc[1] and desc[2] as all-zeros initially.
     */
    vring_packed_set_desc(vr, 0, hdr_phys, sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    __sync_synchronize();

    /* Kick before writing the rest of the chain */
    virtio_pci_kick(dev, 0);

    /* Now write the remaining descriptors (too late) */
    usleep(1000); /* small delay to widen the race window */
    vring_packed_set_desc(vr, 1, data_phys, 512, 1,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 2, status_phys, 1, 2,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    __sync_synchronize();

    usleep(500000);
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0004, VIRTIO_PCI_DEVICE_BLK, test_packed_first_avail_before_rest,
                     "First desc available before subsequent descs ready",
                     VIRTIO_SPEC_V1_2, "2.8.17");
