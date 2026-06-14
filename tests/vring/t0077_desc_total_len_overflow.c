/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0077: desc_chain_total_len_overflow
 *
 * Create a descriptor chain where summing all descriptor lengths
 * overflows a uint32. The individual lengths are valid (large but
 * within 32-bit range), but their sum exceeds 4 GiB. Tests device
 * arithmetic for total transfer size calculations.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_desc_total_len_overflow(struct virtio_dev *dev,
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

    /*
     * Chain: header + two huge data descriptors + status.
     * Each data desc claims 0x80000000 bytes (2 GiB).
     * Sum = 4 GiB + header + status → overflows uint32.
     * The addresses are bogus but we're testing length arithmetic.
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 0x80000000,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, data_phys, 0x80000000,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 3);
    vring_raw_set_desc(vr, 3, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0077, VIRTIO_PCI_DEVICE_BLK, test_desc_total_len_overflow,
              "Descriptor chain total length overflows uint32",
              VIRTIO_SPEC_V1_2, "2.7.5");
