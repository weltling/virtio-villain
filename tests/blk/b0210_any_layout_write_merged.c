/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0210: blk_any_layout_write_hdr_data_merged
 *
 * Virtio 1.x drivers may use any descriptor layout: the 16-byte header
 * is not required to occupy its own descriptor. Submit an OUT (write)
 * request as a two descriptor chain where the readable descriptor
 * carries the 16-byte header followed by the 512 data bytes, then a
 * writable status descriptor.
 *
 * A spec-compliant device must accept this and write a status byte;
 * a device that hardcodes "header is descriptor 0, data is between,
 * status is the last" (the legacy three-descriptor layout) will either
 * mis-parse the header or reject the chain.
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
#define VIRTIO_BLK_S_OK 0

static test_result_t test_blk_any_layout_write_merged(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    /* One contiguous buffer: 16 byte header followed by 512 data bytes. */
    uint8_t *buf = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    struct virtio_blk_outhdr *hdr = (struct virtio_blk_outhdr *)buf;
    hdr->type = VIRTIO_BLK_T_OUT;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(buf + sizeof(*hdr), 0x5a, 512);
    *status = 0xFF;

    uint64_t buf_phys = vv_virt_to_phys(buf);
    uint64_t status_phys = vv_virt_to_phys(status);

    /*
     * desc 0: header + data in one readable descriptor (16 + 512 bytes)
     * desc 1: status (writable, 1 byte)
     */
    vring_raw_set_desc(vr, 0, buf_phys, sizeof(*hdr) + 512,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, status_phys, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    /* Device processed the chain; the status byte must say OK. */
    if (*status != VIRTIO_BLK_S_OK)
        return TEST_FAIL;
    return TEST_PASS;
}

REGISTER_TEST(B0210, VIRTIO_PCI_DEVICE_BLK,
              test_blk_any_layout_write_merged,
              "OUT request with header and data in one readable descriptor",
              VIRTIO_SPEC_V1_2, "5.2.6");
