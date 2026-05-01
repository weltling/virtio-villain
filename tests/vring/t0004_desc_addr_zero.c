/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0004: desc_addr_zero
 *
 * Set a data descriptor's guest physical address to 0 (NULL). Many VMMs
 * map GPA 0 to something valid internally but some treat it as an error.
 * A VMM that blindly translates GPA 0 to a host pointer and writes to
 * it may corrupt page zero or trigger a null-pointer dereference if the
 * host has mmap_min_addr protections.
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

static test_result_t test_desc_addr_zero(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    /*
     * Descriptor chain:
     *   [0] valid header (readable)
     *   [1] data buffer at GPA 0x0 (writable)
     *   [2] status byte (writable, valid)
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0x0ULL, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0004, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_zero,
              "Descriptor with NULL guest physical address",
              VIRTIO_SPEC_V1_2, "2.7.5");
