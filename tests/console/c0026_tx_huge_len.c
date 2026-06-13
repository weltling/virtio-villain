/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0026: transmitq descriptor with huge len over a small backing
 *
 * Spec 5.3 says transmitq descriptors carry guest readable bytes.
 * The spec puts no upper bound on desc.len. A device that walks
 * desc.len without checking it against the actual mapped backing
 * may allocate arbitrary host memory or warn for every byte. The
 * test posts one descriptor with len 1 GiB over a 4 KiB backing
 * and verifies the device does not crash the harness. Outcome of
 * the kick itself is up to the device. PASS, REJECT and WEDGED
 * are all acceptable, as long as the host survives and the next
 * test can still run.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_console_tx_huge_len(struct virtio_dev *dev,
                                              struct vring *vr)
{
    char *buf = (char *)vv_alloc_pages(1);
    memset(buf, 'x', 4096);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 1u << 30, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("r == TEST_FAIL");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST_Q(C0026, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_huge_len,
                "transmitq descriptor with huge len over a small backing",
                VIRTIO_SPEC_V1_2, "5.3", 1);
