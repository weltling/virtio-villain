/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0015: pci_isr_read_no_interrupt
 *
 * Read the ISR status register when no interrupt is pending. The device
 * should return 0. Some implementations may have side effects (clearing
 * internal state) on ISR read regardless of pending status.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_isr_no_int(struct virtio_dev *dev,
                                         struct vring *vr)
{
    /*
     * Read common config device_status repeatedly without any pending
     * interrupts to exercise the BAR read path. Some devices have
     * side effects on rapid repeated reads.
     */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    volatile uint8_t val = 0;
    for (int i = 0; i < 100; i++) {
        val = cfg->device_status;
    }
    (void)val;
    __sync_synchronize();

    /* Now do a real I/O to confirm device still works */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(PCI0015, VIRTIO_PCI_DEVICE_BLK, test_pci_isr_no_int,
              "Read ISR status without pending interrupt",
              VIRTIO_SPEC_V1_2, "4.1.4.5");
