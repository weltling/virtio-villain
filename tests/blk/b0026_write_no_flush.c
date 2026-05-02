/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0026: blk_write_no_flush
 *
 * Issue a WRITE request and immediately read back without an
 * intervening FLUSH. Tests whether the device assumes writes are
 * durable without explicit flush. This is a correctness probe -
 * we verify the device processes both requests without crash.
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

#define VIRTIO_BLK_T_OUT 1
#define VIRTIO_BLK_T_IN  0

static test_result_t test_blk_write_no_flush(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_blk_outhdr *hdr_w = vv_alloc_pages(1);
    struct virtio_blk_outhdr *hdr_r = vv_alloc_pages(1);
    uint8_t *wdata = vv_alloc_pages(1);
    uint8_t *rdata = vv_alloc_pages(1);
    uint8_t *status_w = vv_alloc_pages(1);
    uint8_t *status_r = vv_alloc_pages(1);

    /* Write request: sector 0 */
    hdr_w->type = VIRTIO_BLK_T_OUT;
    hdr_w->ioprio = 0;
    hdr_w->sector = 0;
    memset(wdata, 0x42, 512);
    *status_w = 0xFF;

    /* Read request: sector 0 (no flush in between) */
    hdr_r->type = VIRTIO_BLK_T_IN;
    hdr_r->ioprio = 0;
    hdr_r->sector = 0;
    *status_r = 0xFF;

    uint64_t hdr_w_phys = vv_virt_to_phys(hdr_w);
    uint64_t wdata_phys = vv_virt_to_phys(wdata);
    uint64_t status_w_phys = vv_virt_to_phys(status_w);
    uint64_t hdr_r_phys = vv_virt_to_phys(hdr_r);
    uint64_t rdata_phys = vv_virt_to_phys(rdata);
    uint64_t status_r_phys = vv_virt_to_phys(status_r);

    /* Request 1 (write): descs 0-2 */
    vring_raw_set_desc(vr, 0, hdr_w_phys, sizeof(*hdr_w),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, wdata_phys, 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_w_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    /* Request 2 (read): descs 3-5 */
    vring_raw_set_desc(vr, 3, hdr_r_phys, sizeof(*hdr_r),
                       VRING_DESC_F_NEXT, 4);
    vring_raw_set_desc(vr, 4, rdata_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 5);
    vring_raw_set_desc(vr, 5, status_r_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    /* Both in avail ring */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 3);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0026, VIRTIO_PCI_DEVICE_BLK, test_blk_write_no_flush,
              "Write then read without intervening flush",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
