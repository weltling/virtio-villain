/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0051: modify_inflight_descriptor
 *
 * Modify a descriptor after it has been placed in the available ring
 * and kicked. The spec says once a buffer is exposed to the device,
 * the driver MUST NOT modify it until it appears in the used ring.
 * A VMM that reads descriptor fields multiple times may see torn data.
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

static test_result_t test_modify_inflight(struct virtio_dev *dev,
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

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* Kick to submit */
    virtio_pci_kick(dev, 0);

    /*
     * Immediately mutate the descriptor while device may be processing.
     * Change addr to garbage and len to huge value.
     */
    vring_raw_set_desc(vr, 1, 0xDEADBEEFDEADULL, 0xFFFFFFFF,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    __sync_synchronize();

    /* Also corrupt the header */
    hdr->type = 0xFFFFFFFF;
    hdr->sector = 0xFFFFFFFFFFFFFFFFULL;
    __sync_synchronize();

    usleep(500000);
    __sync_synchronize();

    /* If we get here, VMM survived the torn descriptor read */
    return TEST_PASS;
}

REGISTER_TEST(T0051, VIRTIO_PCI_DEVICE_BLK, test_modify_inflight,
              "Modify descriptor after making available",
              VIRTIO_SPEC_V1_2, "3.3.1");
