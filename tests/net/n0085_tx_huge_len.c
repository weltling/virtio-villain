/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0085: TX descriptor with huge len over a small backing
 *
 * Spec 5.1.6.2 says transmitq carries guest readable frames. The
 * spec puts no upper bound on desc.len. A device that walks
 * desc.len without checking it against the actual mapped backing
 * may translate, copy or allocate using the attacker chosen size.
 * The test posts a virtio_net_hdr followed by one payload
 * descriptor of len 1 GiB over a 4 KiB backing and verifies the
 * device does not crash the harness. PASS, REJECT and WEDGED are
 * all acceptable, as long as the host survives and the next test
 * can still run.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_huge_len(struct virtio_dev *dev)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->num_queues < 2)
        return TEST_SKIP;

    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    memset(payload, 'z', 4096);

    vv_desc(&txvr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(&txvr, 1, payload, 1u << 30, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    test_result_t r = vv_kick_and_wait(dev, &txvr, 1, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("r == TEST_FAIL");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST_DEV_REQUIRES(N0085, VIRTIO_PCI_DEVICE_NET, test_net_tx_huge_len,
                  "TX descriptor with huge len over a small backing",
                  VIRTIO_SPEC_V1_2, "5.1.6.2",
                  0, 2);
