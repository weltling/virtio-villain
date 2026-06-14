/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0054: chain_length_qsize_plus_one
 *
 * Build a descriptor chain of exactly queue_size + 1 entries by having
 * the last slot point to descriptor 0 (which was already used as head).
 * Different from T02 which wraps all slots - this is exactly one over.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_chain_qsize_plus_one(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);

    uint16_t qsz = vr->size;

    /* Slot 0: header */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);

    /* Slots 1..qsz-2: data buffers chained forward */
    for (uint16_t i = 1; i < qsz - 1; i++) {
        vring_raw_set_desc(vr, i, data_phys, 512,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE,
                           i + 1);
    }

    /* Last slot points back to 0, making chain length = qsz + 1 */
    vring_raw_set_desc(vr, qsz - 1, data_phys, 1,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0054, VIRTIO_PCI_DEVICE_BLK, test_chain_qsize_plus_one,
              "Descriptor chain length == queue_size + 1",
              VIRTIO_SPEC_V1_2, "2.7.5.2");
