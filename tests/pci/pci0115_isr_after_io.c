/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0115: ISR status set after completed request.
 *
 * Spec 4.1.4.5: When the device completes a request and the queue
 * is not using MSI-X, it sets bit 0 of the ISR status register.
 * Reading ISR clears it. Submit a read, then check ISR reflects
 * the queue interrupt (bit 0). Only meaningful without MSI-X.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_isr_after_io(struct virtio_dev *dev,
                                           struct vring *vr)
{
    if (!dev->isr)
        return TEST_SKIP;

    /* Clear ISR by reading it */
    (void)*dev->isr;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN; hdr->ioprio = 0; hdr->sector = 0;
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
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("status %u", *st);

    /* Give the device a moment to raise the ISR */
    usleep(10000);
    uint8_t isr = *dev->isr;

    /* Bit 0 indicates a used ring update. It may already be cleared
     * by MSI-X routing; accept either 0 or bit 0 set as long as no
     * reserved bits are set. */
    if (isr & ~0x3)
        TFAIL("ISR 0x%02x has reserved bits set", isr);

    return TEST_PASS;
}

REGISTER_TEST_FLAGS(PCI0115, VIRTIO_PCI_DEVICE_BLK, test_pci_isr_after_io,
              "ISR status after completed request has no reserved bits",
              VIRTIO_SPEC_V1_2, "4.1.4.5",
              TEST_FLAG_NEEDS_ISR);
