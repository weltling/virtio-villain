/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0027: transmitq long chain of moderate len descriptors
 *
 * Spec 5.3 puts no upper bound on the number of descriptors per
 * chain or on the cumulative length of a chain. A device that
 * allocates a fresh host buffer per descriptor without bounding
 * the cumulative size can be driven to large host allocations by
 * one chain. The test posts one chain of 8 descriptors, each
 * with len 1 MiB, all backed by the same 4 KiB page. The host
 * must survive the kick and remain responsive for the next test.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

#define CHAIN_LEN 8
#define DESC_LEN  (1u << 20)

static test_result_t test_console_tx_long_chain(struct virtio_dev *dev,
                                                struct vring *vr)
{
    if ((int)vr->size < CHAIN_LEN + 1)
        return TEST_SKIP;

    char *buf = (char *)vv_alloc_pages(1);
    memset(buf, 'y', 4096);
    uint64_t pa = vv_virt_to_phys(buf);

    for (uint16_t i = 0; i < CHAIN_LEN; i++) {
        uint16_t flags = (i + 1 < CHAIN_LEN) ? VRING_DESC_F_NEXT : 0;
        uint16_t next = (i + 1 < CHAIN_LEN) ? (i + 1) : 0;
        vring_raw_set_desc(vr, i, pa, DESC_LEN, flags, next);
    }
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("r == TEST_FAIL");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST_Q(C0027, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_long_chain,
                "transmitq long chain of moderate len descriptors",
                VIRTIO_SPEC_V1_2, "5.3", 1);
