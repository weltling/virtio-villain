/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0059: ISR status read clears.
 *
 * Spec 4.1.4.5: reading the ISR status register clears it. Read
 * it twice in succession; the second read must return 0.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_pci_isr_clear(struct virtio_dev *dev,
                                        struct vring *vr)
{
    (void)vr;
    if (!dev->isr)
        return TEST_SKIP;
    volatile uint8_t *isr = dev->isr;
    (void)*isr;
    uint8_t second = *isr;
    if (second != 0)
        TFAIL("second != 0");
    return TEST_PASS;
}

REGISTER_TEST(PCI0059, 0, test_pci_isr_clear,
              "ISR status clears on read",
              VIRTIO_SPEC_V1_2, "4.1.4.5");
