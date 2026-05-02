/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0029: blk_header_split_descriptors
 *
 * Submit a block request where the 16-byte header is split across two
 * separate readable descriptors (8 bytes each). The spec defines the
 * header as a single contiguous structure. A VMM that reads the header
 * from only the first descriptor will get a truncated/corrupt header.
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

static test_result_t test_blk_header_split(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    /*
     * Split the 16-byte header into two 8-byte descriptors:
     * desc 0: first 8 bytes (type + ioprio)
     * desc 1: next 8 bytes (sector)
     * desc 2: data (writable)
     * desc 3: status (writable)
     */
    vring_raw_set_desc(vr, 0, hdr_phys, 8, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, hdr_phys + 8, 8, VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 3);
    vring_raw_set_desc(vr, 3, status_phys, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0029, VIRTIO_PCI_DEVICE_BLK, test_blk_header_split,
              "Header split across two readable descriptors",
              VIRTIO_SPEC_V1_2, "5.2.6");
