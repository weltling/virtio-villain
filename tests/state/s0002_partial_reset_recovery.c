/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0002: partial_reset_recovery
 *
 * Set the FAILED status bit, then without resetting the device, attempt
 * to continue using the existing virtqueue. The spec says the driver
 * MUST later reset the device before attempting to re-initialize (3.1.1).
 *
 * A VMM that doesn't enforce this may allow a "failed" device to process
 * requests, breaking isolation guarantees.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_partial_reset_recovery(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    /* Device is in DRIVER_OK. Set FAILED bit. */
    dev->common->device_status |= VIRTIO_STATUS_FAILED;
    __sync_synchronize();
    usleep(10000);

    /*
     * Now attempt to clear FAILED and restore DRIVER_OK without
     * going through a full reset (status=0). This is the violation.
     */
    dev->common->device_status = VIRTIO_STATUS_ACKNOWLEDGE |
                                 VIRTIO_STATUS_DRIVER |
                                 VIRTIO_STATUS_FEATURES_OK |
                                 VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    usleep(10000);

    /* Attempt I/O on the "recovered" device */
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

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0002, VIRTIO_PCI_DEVICE_BLK, test_partial_reset_recovery,
              "Continue using virtqueue after FAILED without reset",
              VIRTIO_SPEC_V1_2, "3.1.1");
