/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0078: used_ring_element_len_exceeds_buffer
 *
 * Submit a read, then verify that the used ring element's `len` field
 * does not exceed the total writable buffer space provided. A device
 * reporting more bytes written than the buffer can hold indicates a
 * bounds error.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_used_ring_len_overflow(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    /* Provide exactly 512 + 1 writable bytes */
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

    /* Check used element len */
    __sync_synchronize();
    uint32_t used_len = vr->used->ring[0].len;
    uint32_t max_writable = 512 + 1; /* data + status */

    if (used_len > max_writable)
        TFAIL("used_len > max_writable"); /* device reports more written than buffer */

    return TEST_PASS;
}

REGISTER_TEST(T0078, VIRTIO_PCI_DEVICE_BLK, test_used_ring_len_overflow,
              "Used ring element len must not exceed writable buffer size",
              VIRTIO_SPEC_V1_2, "2.7.8");
