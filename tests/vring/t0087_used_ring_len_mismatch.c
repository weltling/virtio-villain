/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0087: Submit a 512-byte read, then check if used ring reports len > 512.
 *
 * Spec 2.7.4.2: The device MUST NOT write more than the total
 * descriptor length. Verify that used_elem.len does not exceed
 * what was offered in the writable descriptors.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_used_ring_len_mismatch(struct virtio_dev *dev,
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
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* Check used ring entry: writable bytes = 512 (data) + 1 (status) = 513 */
    __sync_synchronize();
    uint32_t used_len = vr->used->ring[0].len;

    /* Device must not report more bytes written than offered (513 max) */
    if (used_len > 513)
        TFAIL("used_len > 513");

    return TEST_PASS;
}

REGISTER_TEST(T0087, VIRTIO_PCI_DEVICE_BLK, test_used_ring_len_mismatch,
              "Used ring len must not exceed offered writable bytes",
              VIRTIO_SPEC_V1_2, "2.7.4.2");
