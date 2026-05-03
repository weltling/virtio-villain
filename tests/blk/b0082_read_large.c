/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0082: Read request with data spanning multiple pages (large I/O)
 *
 * Test that the device handles a large read correctly when the
 * data buffer is 64 sectors (32KB) — typical large block I/O.
 * While not a spec violation, large I/Os stress different code
 * paths in VMM backend buffer handling.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_blk_read_large(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    /* 32KB = 8 pages */
    uint8_t *data = vv_alloc_pages(8);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    /* 32KB read (64 sectors) */
    vring_raw_set_desc(vr, 1, data_phys, 32768,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0082, VIRTIO_PCI_DEVICE_BLK, test_blk_read_large,
              "Read 32KB (64 sectors) in a single request",
              VIRTIO_SPEC_V1_2, "5.2.6");
