/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0053: interleave_read_write_descs
 *
 * Create a descriptor chain that interleaves device-readable and
 * device-writable descriptors: readable -> writable -> readable.
 * Spec 2.7.4.2: device-writable descriptors MUST NOT follow
 * device-readable ones (they must be grouped: all readable first,
 * then all writable).
 */
#include "lib/virtio_test.h"

static test_result_t test_interleave_rw_descs(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *buf1 = vv_alloc_pages(1);
    uint8_t *buf2 = vv_alloc_pages(1);
    uint8_t *buf3 = vv_alloc_pages(1);

    /*
     * Chain: [0] header (readable) -> [1] data (writable) ->
     *        [2] extra readable -> [3] status (writable)
     * The readable desc [2] after writable [1] violates ordering.
     */
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, buf1, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    /* Readable desc after writable - violation */
    vv_desc(vr, 2, buf2, 16, VRING_DESC_F_NEXT, 3);
    vv_desc(vr, 3, buf3, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0053, VIRTIO_PCI_DEVICE_BLK, test_interleave_rw_descs,
              "Interleaved readable/writable descriptors",
              VIRTIO_SPEC_V1_2, "2.7.4.2");
