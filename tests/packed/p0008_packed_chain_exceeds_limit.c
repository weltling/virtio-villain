/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0008: packed_chain_exceeds_device_limit
 *
 * Create a descriptor chain with more entries than the device would
 * need for a single request. For a block read this is header + data +
 * status = 3, but we provide 10 chained data descriptors.
 * Spec 2.8.19: chain MUST NOT exceed what device allows.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_chain_exceeds_limit(struct virtio_dev *dev,
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

    /* Header */
    vring_packed_set_desc(vr, 0, hdr_phys, sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);

    /* 10 data descriptors (excessive for a single-sector read) */
    for (int i = 1; i <= 10; i++) {
        vring_packed_set_desc(vr, i, data_phys, 512, i,
                              VRING_PACKED_DESC_F_NEXT |
                              VRING_PACKED_DESC_F_WRITE);
        vring_packed_advance(vr);
    }

    /* Status */
    vring_packed_set_desc(vr, 11, status_phys, 1, 11,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, 0, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0008, VIRTIO_PCI_DEVICE_BLK, test_packed_chain_exceeds_limit,
                     "Packed chain longer than device expects",
                     VIRTIO_SPEC_V1_2, "2.8.19");
