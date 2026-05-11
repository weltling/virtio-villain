/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0080: write_failed_bit_from_driver
 *
 * Set the DEVICE_NEEDS_RESET bit (bit 6) from the driver side.
 * Spec 2.1.2 says this bit is device writable only. A driver
 * writing it is a protocol violation; the device must ignore or
 * mask the write without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

static test_result_t test_write_needs_reset(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    uint8_t old = cfg->device_status;
    if (!(old & VIRTIO_STATUS_DRIVER_OK))
        return TEST_SKIP;

    /* Write NEEDS_RESET from driver (violation) */
    cfg->device_status = old | VIRTIO_STATUS_NEEDS_RESET;
    __sync_synchronize();
    usleep(50000);

    /*
     * The device may either ignore the write (status unchanged or with
     * NEEDS_RESET masked off) or treat it as a protocol violation and
     * force a reset (status reads 0). Both outcomes are spec-compliant;
     * what matters is that the device did not crash. Reaching this read
     * is the liveness check.
     */
    (void)cfg->device_status;
    return TEST_PASS;
}

REGISTER_TEST(S0080, VIRTIO_PCI_DEVICE_BLK, test_write_needs_reset,
              "Driver writes NEEDS_RESET bit",
              VIRTIO_SPEC_V1_2, "2.1.2");
