/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0125: Descriptor chain loop in block request.
 *
 * Create a request where the data descriptor's next index points back
 * to the header, forming a loop. The device must detect the cycle.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_blk_desc_chain_loop(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);

    /* desc 0 -> desc 1 -> desc 0 (loop) */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0125, VIRTIO_PCI_DEVICE_BLK, test_blk_desc_chain_loop,
              "Descriptor chain loop in request",
              VIRTIO_SPEC_V1_2, "5.2.6");
