/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0009: Console receive with zero-length writable descriptor.
 *
 * Submit a writable descriptor of length 0 to the receiveq. The
 * device must not write past the (non-existent) buffer when host
 * input arrives. On a quiet console the expected outcome is
 * REJECT (no completion observed within the timeout).
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_rx_zero_len(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 0, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0009, VIRTIO_PCI_DEVICE_CONSOLE, test_console_rx_zero_len,
              "Console receive zero-length writable descriptor",
              VIRTIO_SPEC_V1_2, "5.3");
