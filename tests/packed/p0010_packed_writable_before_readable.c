/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0010: packed_writable_before_readable
 *
 * In a packed descriptor chain, place a device-writable descriptor
 * before a device-readable one. Like split rings, packed descriptors
 * must have all readable buffers before writable ones in a chain.
 * Spec 2.8.17: device-writable buffers MUST come after all
 * device-readable buffers.
 */
#include "lib/virtio_test.h"

static test_result_t test_packed_writable_before_readable(
    struct virtio_dev *dev, struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *extra = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    /*
     * Chain: [0] header (readable) -> [1] data (WRITABLE) ->
     *        [2] extra (READABLE) -> [3] status (WRITABLE)
     * The readable desc [2] after writable [1] violates ordering.
     */
    vring_packed_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 1, vv_virt_to_phys(data), 512, 1,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    /* Readable after writable - violation */
    vring_packed_set_desc(vr, 2, vv_virt_to_phys(extra), 16, 2,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 3, vv_virt_to_phys(status), 1, 3,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, 0, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0010, VIRTIO_PCI_DEVICE_BLK, test_packed_writable_before_readable,
                     "Writable desc before readable in packed chain",
                     VIRTIO_SPEC_V1_2, "2.8.17");
