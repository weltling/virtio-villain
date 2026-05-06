/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0003: Console transmit with WRITE flag set.
 *
 * Submit a transmitq descriptor with VRING_DESC_F_WRITE set. The
 * driver should never set this on the TX queue because the device
 * only reads outgoing data. The device must handle the misuse
 * gracefully without crashing.
 *
 * Spec 2.7.13.1 mandates the device only treat as device-writable
 * those descriptors with the WRITE flag, but does not forbid the
 * driver from setting it on a TX queue. The device must not
 * misinterpret the buffer.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_tx_write_flag(struct virtio_dev *dev,
                                                struct vring *vr)
{
    char *buf = (char *)vv_alloc_pages(1);
    memcpy(buf, "x\n", 2);

    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 2, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0003, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_write_flag,
                "Console transmit with WRITE flag set",
                VIRTIO_SPEC_V1_2, "5.3", 1);
