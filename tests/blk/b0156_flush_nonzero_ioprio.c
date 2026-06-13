/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0156: blk_flush_nonzero_ioprio
 *
 * Spec 5.2.6.2 says ioprio is unused by the device for FLUSH.
 * Submit a FLUSH whose request header carries a non zero ioprio
 * value. The device must still complete the flush successfully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_flush_nonzero_ioprio(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_FLUSH;
    hdr->ioprio = 0xCAFEBABE;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0156, VIRTIO_PCI_DEVICE_BLK, test_blk_flush_nonzero_ioprio,
              "FLUSH with non zero ioprio still completes",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
