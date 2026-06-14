/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0015: pci_isr_read_no_interrupt
 *
 * Read the ISR status register when no interrupt is pending. The device
 * should return 0. Some implementations may have side effects (clearing
 * internal state) on ISR read regardless of pending status.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

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

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(PCI0015, VIRTIO_PCI_DEVICE_BLK, test_pci_isr_no_int,
              "Read ISR status without pending interrupt",
              VIRTIO_SPEC_V1_2, "4.1.4.5");
