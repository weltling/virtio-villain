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
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_interleave_rw_descs(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *buf1 = vv_alloc_pages(1);
    uint8_t *buf2 = vv_alloc_pages(1);
    uint8_t *buf3 = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t buf1_phys = vv_virt_to_phys(buf1);
    uint64_t buf2_phys = vv_virt_to_phys(buf2);
    uint64_t buf3_phys = vv_virt_to_phys(buf3);

    /*
     * Chain: [0] header (readable) -> [1] data (writable) ->
     *        [2] extra readable -> [3] status (writable)
     * The readable desc [2] after writable [1] violates ordering.
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, buf1_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    /* Readable desc after writable - violation */
    vring_raw_set_desc(vr, 2, buf2_phys, 16,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, buf3_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0053, VIRTIO_PCI_DEVICE_BLK, test_interleave_rw_descs,
              "Interleaved readable/writable descriptors",
              VIRTIO_SPEC_V1_2, "2.7.4.2");
