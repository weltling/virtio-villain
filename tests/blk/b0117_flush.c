/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0117: blk_flush
 *
 * Submit a VIRTIO_BLK_T_FLUSH request to ensure data durability.
 * Spec 5.2.6.1: If VIRTIO_BLK_F_FLUSH is negotiated, the device
 * MUST complete the flush before reporting used.
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

#define VIRTIO_BLK_T_FLUSH 4

static test_result_t test_blk_flush(struct virtio_dev *dev,
                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_FLUSH;
    hdr->ioprio = 0;
    hdr->sector = 0;  /* sector is ignored for flush */

    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* Flush: header (device-readable) + status (device-writable) */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0117, VIRTIO_PCI_DEVICE_BLK, test_blk_flush,
              "Flush (write barrier) request",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
