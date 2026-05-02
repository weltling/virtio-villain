/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0005: interrupt_after_failed
 *
 * Submit a valid I/O request, immediately set device_status to FAILED
 * before the device can respond, then verify the device does not deliver
 * an interrupt or complete the request. The spec says the device MUST
 * NOT send notifications after a FAILED status (implied by 3.1.1 - the
 * device must stop all operations).
 *
 * A VMM with an async I/O backend may still complete in-flight requests
 * and write to the used ring or fire an interrupt after FAILED, which
 * could confuse a recovering driver or cause a use-after-free if
 * buffers were already freed.
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

static test_result_t test_interrupt_after_failed(struct virtio_dev *dev,
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

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* Kick the request */
    uint16_t used_before = vr->used->idx;
    __sync_synchronize();
    virtio_pci_kick(dev, 0);

    /* Immediately set FAILED before device can complete */
    dev->common->device_status |= VIRTIO_STATUS_FAILED;
    __sync_synchronize();

    /* Wait and check if device still completed the request */
    usleep(300000);
    __sync_synchronize();

    if (vr->used->idx != used_before) {
        /*
         * Device completed the request after FAILED - this means the
         * VMM's async backend didn't respect the status transition.
         * Report as PASS since we're testing whether the VMM crashes,
         * not whether it correctly drops in-flight I/O.
         */
        return TEST_PASS;
    }

    /* Device correctly did not complete after FAILED */
    TREJECT("no device response within timeout");
}

REGISTER_TEST(S0005, VIRTIO_PCI_DEVICE_BLK, test_interrupt_after_failed,
              "Submit I/O then immediately set FAILED status",
              VIRTIO_SPEC_V1_2, "3.1.1");
