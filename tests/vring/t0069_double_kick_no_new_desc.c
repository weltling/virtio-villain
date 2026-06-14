/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0069: double_kick_no_new_desc
 *
 * Submit a valid request, wait for completion, then kick again without
 * posting anything new. The device must handle the duplicate notification
 * gracefully without crashing or re-processing old descriptors.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_double_kick(struct virtio_dev *dev,
                                      struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* First kick - device should process the request */
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r; /* unexpected: device didn't process valid read */

    /* Second kick - nothing new in the ring */
    return vv_kick_expect_reject(dev, vr, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0069, VIRTIO_PCI_DEVICE_BLK, test_double_kick,
              "Duplicate kick after request completion",
              VIRTIO_SPEC_V1_2, "2.7.13");
