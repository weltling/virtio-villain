/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0002: desc_chain_too_long
 *
 * Build a descriptor chain that exceeds queue_size entries by wrapping
 * the next index back to 0 after filling every slot. A compliant VMM
 * must stop walking the chain at the queue_size boundary rather than
 * following it indefinitely.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

/* virtio-blk request header */
static test_result_t test_desc_chain_too_long(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0, 512);

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);

    uint16_t qsz = vr->size;

    /*
     * Fill every descriptor slot as a chain:
     *   [0] header -> [1] -> [2] -> ... -> [qsz-1] -> [0] (wraps!)
     *
     * Slot 0: request header (readable)
     * Slots 1..qsz-2: data (writable)
     * Slot qsz-1: points back to 0 (making chain length qsz+1)
     *
     * A safe VMM must cap the walk at qsz entries.
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);

    for (uint16_t i = 1; i < qsz - 1; i++) {
        vring_raw_set_desc(vr, i, data_phys, 512,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE,
                           i + 1);
    }

    /* Last slot wraps back to descriptor 0 */
    vring_raw_set_desc(vr, qsz - 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 0);

    /* Submit head 0 */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0002, VIRTIO_PCI_DEVICE_BLK, test_desc_chain_too_long,
              "Descriptor chain exceeds queue size",
              VIRTIO_SPEC_V1_2, "2.7.5");
