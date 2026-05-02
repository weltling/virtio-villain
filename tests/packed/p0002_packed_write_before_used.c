/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0002: packed_write_before_used
 *
 * Write new data into a descriptor before the device has marked it
 * used (USED bit not yet set). The driver races the device by
 * resubmitting over an in-flight descriptor.
 * Spec 2.8.16: driver MUST NOT write a descriptor before observing
 * the USED bit.
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

static test_result_t test_packed_write_before_used(struct virtio_dev *dev,
                                                   struct vring_packed *vr)
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

    uint8_t initial_wrap = vr->wrap_counter;

    /* Submit first request */
    vring_packed_set_desc(vr, 0, hdr_phys, sizeof(*hdr), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 1, data_phys, 512, 1,
                          VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);
    vring_packed_set_desc(vr, 2, status_phys, 1, 2,
                          VRING_PACKED_DESC_F_WRITE);
    vring_packed_advance(vr);

    __sync_synchronize();
    virtio_pci_kick(dev, 0);

    /*
     * Without waiting for USED bit, immediately overwrite desc[0]
     * with a new request - racing with in-flight processing.
     */
    hdr->sector = 1;
    vr->desc[0].addr = hdr_phys;
    vr->desc[0].len = sizeof(*hdr);
    __sync_synchronize();

    /* Now kick again */
    virtio_pci_kick(dev, 0);
    usleep(500000);

    (void)initial_wrap;
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0002, VIRTIO_PCI_DEVICE_BLK, test_packed_write_before_used,
                     "Write descriptor before observing USED bit",
                     VIRTIO_SPEC_V1_2, "2.8.16");
