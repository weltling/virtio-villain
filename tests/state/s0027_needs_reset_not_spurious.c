/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0027: NEEDS_RESET device-initiated (spec 2.1.2)
 *
 * After normal init, check that NEEDS_RESET is NOT set. Then
 * submit a potentially problematic request and verify the device
 * doesn't spuriously set NEEDS_RESET during normal operation.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_needs_reset_not_spurious(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* After init, NEEDS_RESET must not be set */
    uint8_t status = cfg->device_status;
    if (status & VIRTIO_STATUS_NEEDS_RESET)
        TFAIL("status & VIRTIO_STATUS_NEEDS_RESET");

    /* Submit a valid read request */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);

    /* After a valid request, NEEDS_RESET should still not be set */
    status = cfg->device_status;
    if (status & VIRTIO_STATUS_NEEDS_RESET)
        TFAIL("status & VIRTIO_STATUS_NEEDS_RESET");

    /* If the read succeeded, double check status byte */
    if (r == TEST_PASS && *st != 0)
        TFAIL("r == TEST_PASS && *st != 0"); /* bad status despite used ring advance */

    return TEST_PASS;
}

REGISTER_TEST(S0027, VIRTIO_PCI_DEVICE_BLK, test_needs_reset_not_spurious,
              "NEEDS_RESET not spuriously set during normal I/O",
              VIRTIO_SPEC_V1_2, "2.1.2");
