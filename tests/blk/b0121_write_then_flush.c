/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0121: blk_write_then_flush
 *
 * Write data then issue a flush to exercise the complete write path.
 * Spec 5.2.6.1: After flush completes, all prior writes MUST be
 * durable on the backing store.
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

#define VIRTIO_BLK_T_OUT   1
#define VIRTIO_BLK_T_FLUSH 4

static test_result_t test_blk_write_then_flush(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    /* First: write one sector */
    hdr->type = VIRTIO_BLK_T_OUT;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0xAB, 512);
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* Second: flush */
    struct virtio_blk_outhdr *flush_hdr = vv_alloc_pages(1);
    uint8_t *flush_status = vv_alloc_pages(1);

    flush_hdr->type = VIRTIO_BLK_T_FLUSH;
    flush_hdr->ioprio = 0;
    flush_hdr->sector = 0;
    *flush_status = 0xFF;

    uint64_t flush_hdr_phys = vv_virt_to_phys(flush_hdr);
    uint64_t flush_status_phys = vv_virt_to_phys(flush_status);

    vring_raw_set_desc(vr, 0, flush_hdr_phys, sizeof(*flush_hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, flush_status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0121, VIRTIO_PCI_DEVICE_BLK, test_blk_write_then_flush,
              "Write followed by flush",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
