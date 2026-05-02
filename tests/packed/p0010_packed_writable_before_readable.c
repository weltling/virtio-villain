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
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_packed_writable_before_readable(
    struct virtio_dev *dev, struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *extra = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t extra_phys = vv_virt_to_phys(extra);
    uint64_t status_phys = vv_virt_to_phys(status);

    /*
     * Chain: [0] header (readable) -> [1] data (WRITABLE) ->
     *        [2] extra (READABLE) -> [3] status (WRITABLE)
     * The readable desc [2] after writable [1] violates ordering.
     */
    vring_packed_set_desc(vr, 0, hdr_phys, sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 1, data_phys, 512, 1,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    /* Readable after writable - violation */
    vring_packed_set_desc(vr, 2, extra_phys, 16, 2,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 3, status_phys, 1, 3,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    return vv_kick_and_wait_packed(dev, vr, 0, 0, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0010, VIRTIO_PCI_DEVICE_BLK, test_packed_writable_before_readable,
                     "Writable desc before readable in packed chain",
                     VIRTIO_SPEC_V1_2, "2.8.17");
