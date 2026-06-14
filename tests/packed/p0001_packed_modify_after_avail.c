/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0001: packed_modify_after_avail
 *
 * Make a descriptor available (set AVAIL flag), then immediately
 * rewrite the descriptor's address and length. Tests whether the
 * device reads the descriptor atomically after observing the AVAIL bit.
 * Spec 2.8.16: driver MUST NOT modify a descriptor after making it
 * available.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_modify_after_avail(struct virtio_dev *dev,
                                                    struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);
    uint8_t *bogus = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);
    uint64_t bogus_phys = vv_virt_to_phys(bogus);

    /* Make a valid 3-descriptor chain available */
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

    /* Immediately modify the data descriptor after making available */
    vr->desc[1].addr = bogus_phys;
    vr->desc[1].len = 4096;
    __sync_synchronize();

    usleep(500000);
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0001, VIRTIO_PCI_DEVICE_BLK, test_packed_modify_after_avail,
                     "Modify descriptor after setting AVAIL flag",
                     VIRTIO_SPEC_V1_2, "2.8.16");
