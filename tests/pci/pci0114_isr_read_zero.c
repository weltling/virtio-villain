/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0114: ISR status reads zero with no pending interrupt.
 *
 * Spec 4.1.4.5: The ISR status register reads the interrupt
 * reason. When no interrupt is pending it should read 0.
 * Read ISR after init (before any I/O) and verify zero.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_pci_isr_zero(struct virtio_dev *dev,
                                       struct vring *vr)
{
    (void)vr;

    if (!dev->isr)
        return TEST_SKIP;

    /* Read ISR before any I/O; should be 0 */
    uint8_t isr = *dev->isr;

    if (isr != 0)
        TFAIL("ISR 0x%02x before any I/O, expected 0", isr);

    return TEST_PASS;
}

REGISTER_TEST_FLAGS(PCI0114, VIRTIO_PCI_DEVICE_BLK, test_pci_isr_zero,
              "ISR reads zero with no pending interrupt",
              VIRTIO_SPEC_V1_2, "4.1.4.5",
              TEST_FLAG_NEEDS_ISR);
