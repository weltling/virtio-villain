/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0061: desc_addr_and_len_zero
 *
 * Create a descriptor with both addr=0 and len=0. This is a degenerate
 * case - the descriptor contributes nothing to the chain but a VMM
 * that dereferences addr without checking len may fault.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_desc_addr_and_len_zero(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* desc 0: valid header */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    /* desc 1: addr=0, len=0, writable - degenerate data descriptor */
    vring_raw_set_desc(vr, 1, 0, 0,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    /* desc 2: status */
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0061, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_and_len_zero,
              "Descriptor with addr=0 and len=0 simultaneously",
              VIRTIO_SPEC_V1_2, "2.7.5");
