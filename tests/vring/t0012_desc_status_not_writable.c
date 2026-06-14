/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0012: desc_status_not_writable
 *
 * Submit a request where the status descriptor lacks the WRITE flag.
 * The device writes the completion status byte into this descriptor;
 * without WRITE it should be rejected.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_desc_status_not_writable(struct virtio_dev *dev,
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
     * Descriptor chain:
     *   [0] header (readable) - correct
     *   [1] data (writable) - correct
     *   [2] status (readable!) - WRONG, device writes status here
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       0, 0); /* no WRITE flag */

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0012, VIRTIO_PCI_DEVICE_BLK, test_desc_status_not_writable,
              "Status descriptor without WRITE flag",
              VIRTIO_SPEC_V1_2, "5.2.6");
