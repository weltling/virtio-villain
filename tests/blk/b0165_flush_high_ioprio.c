/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0165: FLUSH with a non zero ioprio field in the header.
 *
 * Spec 5.2.6: The ioprio field of virtio_blk_outhdr is advisory
 * for non I/O ops. Submit a FLUSH with ioprio=0xFFFFFFFF and the
 * device must ignore the field and complete the flush normally,
 * not treat it as a request length or sector hint.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_flush_high_ioprio(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_FLUSH;
    hdr->ioprio = 0xFFFFFFFFu;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0165, VIRTIO_PCI_DEVICE_BLK,
              test_blk_flush_high_ioprio,
              "FLUSH with non zero ioprio in the header",
              VIRTIO_SPEC_V1_2, "5.2.6");
