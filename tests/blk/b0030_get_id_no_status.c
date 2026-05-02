/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0030: blk_get_id_no_status
 *
 * Submit a GET_ID request with a correct 20-byte data buffer but with
 * no status descriptor. The device needs the status byte to report
 * success/failure; without it the chain is malformed.
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

#define VIRTIO_BLK_T_GET_ID 8

static test_result_t test_blk_get_id_no_status(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_GET_ID;
    hdr->ioprio = 0;
    hdr->sector = 0;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);

    /* Header (readable) -> 20-byte data (writable) but NO status desc */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 20, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0030, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id_no_status,
              "GET_ID with 20-byte data but missing status descriptor",
              VIRTIO_SPEC_V1_2, "5.2.6");
