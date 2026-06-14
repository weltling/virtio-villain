/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0006: desc_len_zero
 *
 * Submit a request where the data descriptor has len = 0. Some VMMs
 * may not handle zero-length transfers, leading to division by zero
 * or assertion failures when computing sector counts.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_desc_len_zero(struct virtio_dev *dev,
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
     *   [0] header (readable)
     *   [1] data buffer with len = 0 (writable)
     *   [2] status byte (writable)
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, hdr_phys, 0,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0006, VIRTIO_PCI_DEVICE_BLK, test_desc_len_zero,
              "Data descriptor with len = 0",
              VIRTIO_SPEC_V1_2, "2.7.5");
