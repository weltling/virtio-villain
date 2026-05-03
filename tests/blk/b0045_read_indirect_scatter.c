/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0045: blk_read_indirect_scatter
 *
 * Submit a READ using an indirect descriptor table with chained
 * data descriptors (scatter-gather). The data is split across
 * 4 separate 128-byte buffers instead of one 512-byte buffer.
 * Tests the device's handling of indirect + multi-buffer reads.
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

static test_result_t test_blk_read_indirect_scatter(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data0 = vv_alloc_pages(1);
    uint8_t *data1 = vv_alloc_pages(1);
    uint8_t *data2 = vv_alloc_pages(1);
    uint8_t *data3 = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t d0_phys = vv_virt_to_phys(data0);
    uint64_t d1_phys = vv_virt_to_phys(data1);
    uint64_t d2_phys = vv_virt_to_phys(data2);
    uint64_t d3_phys = vv_virt_to_phys(data3);
    uint64_t status_phys = vv_virt_to_phys(status);

    /*
     * Build indirect descriptor table:
     *   [0] header (readable)
     *   [1] data0 128 bytes (writable) -> next [2]
     *   [2] data1 128 bytes (writable) -> next [3]
     *   [3] data2 128 bytes (writable) -> next [4]
     *   [4] data3 128 bytes (writable) -> next [5]
     *   [5] status (writable)
     */
    struct vring_desc *indirect = vv_alloc_pages(1);
    indirect[0].addr = hdr_phys;
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 1;

    indirect[1].addr = d0_phys;
    indirect[1].len = 128;
    indirect[1].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
    indirect[1].next = 2;

    indirect[2].addr = d1_phys;
    indirect[2].len = 128;
    indirect[2].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
    indirect[2].next = 3;

    indirect[3].addr = d2_phys;
    indirect[3].len = 128;
    indirect[3].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
    indirect[3].next = 4;

    indirect[4].addr = d3_phys;
    indirect[4].len = 128;
    indirect[4].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
    indirect[4].next = 5;

    indirect[5].addr = status_phys;
    indirect[5].len = 1;
    indirect[5].flags = VRING_DESC_F_WRITE;
    indirect[5].next = 0;

    uint64_t indirect_phys = vv_virt_to_phys(indirect);

    /* Main ring: single indirect descriptor */
    vring_raw_set_desc(vr, 0, indirect_phys, 6 * 16,
                       VRING_DESC_F_INDIRECT, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0045, VIRTIO_PCI_DEVICE_BLK, test_blk_read_indirect_scatter,
              "READ with indirect scatter-gather (4x128 byte buffers)",
              VIRTIO_SPEC_V1_2, "5.2.6");
