/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0033: Submit a packed chain of 8 descriptors (scatter/gather) for
 * a read operation.
 *
 * Spec 2.8.6: Tests packed multi-descriptor chain handling with
 * NEXT flags linking descriptors in sequence. The data is split
 * across 6 separate 512-byte buffers for scatter/gather.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define NUM_DATA_BUFS 6

static test_result_t test_packed_scatter_gather(struct virtio_dev *dev,
                                                struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data[NUM_DATA_BUFS];
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    for (int i = 0; i < NUM_DATA_BUFS; i++)
        data[i] = vv_alloc_pages(1);

    uint8_t wrap = vr->wrap_counter;
    uint16_t start_idx = vr->next_avail;

    /* Desc 0: header (readable) with NEXT */
    vring_packed_set_desc(vr, vr->next_avail, vv_virt_to_phys(hdr),
                          sizeof(*hdr), 0, VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);

    /* Desc 1-6: scatter data buffers (writable) with NEXT */
    for (int i = 0; i < NUM_DATA_BUFS; i++) {
        uint16_t flags = VRING_PACKED_DESC_F_WRITE | VRING_PACKED_DESC_F_NEXT;
        vring_packed_set_desc(vr, vr->next_avail, vv_virt_to_phys(data[i]),
                              512, 0, flags);
        vring_packed_advance(vr);
    }

    /* Desc 7: status (writable, no NEXT) */
    vring_packed_set_desc(vr, vr->next_avail, vv_virt_to_phys(status),
                          1, 0, VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, start_idx, wrap,
                                   VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0033, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_scatter_gather,
                     "Packed 8-descriptor scatter/gather read chain",
                     VIRTIO_SPEC_V1_2, "2.8.6");
