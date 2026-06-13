/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0025: 32 sequential small TX bursts on the transmitq
 *
 * Spec 5.3 says the transmitq accepts read only payload from the
 * guest. A device that mishandles back to back kicks may drop or
 * reorder requests. Submit 32 small payloads one at a time and
 * verify each one completes before submitting the next.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_console_tx_burst(struct virtio_dev *dev,
                                           struct vring *vr)
{
    int n = 32;
    if ((int)vr->size < n + 2)
        n = vr->size - 2;
    if (n < 4)
        return TEST_SKIP;

    for (int i = 0; i < n; i++) {
        char *buf = (char *)vv_alloc_pages(1);
        buf[0] = 'a' + (i % 26);
        buf[1] = '\n';
        vring_raw_set_desc(vr, i, vv_virt_to_phys(buf), 2, 0, 0);
        vring_raw_set_avail(vr, i, i);
        vring_raw_set_avail_idx(vr, i + 1);

        test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
        if (r != TEST_PASS)
            return r;
    }
    return TEST_PASS;
}

REGISTER_TEST_Q(C0025, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_burst,
                "32 small sequential TX requests on transmitq",
                VIRTIO_SPEC_V1_2, "5.3", 1);
