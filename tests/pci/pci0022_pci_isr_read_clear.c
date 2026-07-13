/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0022: pci_isr_read_clear_semantics
 *
 * Verify that reading the ISR status register clears the queue
 * interrupt bit. Submit a request, wait for completion, read ISR
 * (expect queue bit set), read again (expect cleared).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_isr_read_clear(struct virtio_dev *dev,
                                             struct vring *vr)
{
    if (!dev->isr)
        return TEST_SKIP;

    /* Clear any pending ISR state */
    (void)*dev->isr;
    __sync_synchronize();

    /* Submit a simple read request */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* First ISR read: should have queue bit set */
    __sync_synchronize();
    uint8_t isr1 = *dev->isr;

    /* Second ISR read: should be cleared (read-to-clear) */
    __sync_synchronize();
    uint8_t isr2 = *dev->isr;

    if (!(isr1 & VIRTIO_PCI_ISR_QUEUE))
        TREJECT("!(isr1 & VIRTIO_PCI_ISR_QUEUE)"); /* device didn't set queue bit */

    if (isr2 & VIRTIO_PCI_ISR_QUEUE)
        TFAIL("isr2 & VIRTIO_PCI_ISR_QUEUE"); /* bit wasn't cleared by read */

    return TEST_PASS;
}

REGISTER_TEST_FLAGS(PCI0022, VIRTIO_PCI_DEVICE_BLK, test_pci_isr_read_clear,
              "ISR register read-to-clear semantics for queue bit",
              VIRTIO_SPEC_V1_2, "4.1.4.5",
              TEST_FLAG_NEEDS_ISR);
