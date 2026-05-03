/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0039: net_rx_mixed_rw_descriptors
 *
 * Submit an RX buffer chain where some descriptors are readable
 * (wrong direction) mixed with writable ones. The device should
 * reject readable descriptors in an RX queue or at minimum not
 * write data into a readable buffer.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_rx_mixed_rw(struct virtio_dev *dev,
                                          struct vring *vr)
{
    (void)vr; /* TX queue - we need RX queue 0 */

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;
    if (nq < 2)
        return TEST_SKIP;

    /* RX is queue 0 for net */
    struct vring rxvr;
    vring_alloc(&rxvr, 16);
    vring_attach(dev, &rxvr, 0);

    uint8_t *buf0 = vv_alloc_pages(1);
    uint8_t *buf1 = vv_alloc_pages(1);
    uint8_t *buf2 = vv_alloc_pages(1);

    uint64_t b0_phys = vv_virt_to_phys(buf0);
    uint64_t b1_phys = vv_virt_to_phys(buf1);
    uint64_t b2_phys = vv_virt_to_phys(buf2);

    /*
     * Chain: [0] writable 512B -> [1] READABLE 512B -> [2] writable 512B
     * Descriptor 1 is wrong direction for an RX queue.
     */
    vring_raw_set_desc(&rxvr, 0, b0_phys, 512,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&rxvr, 1, b1_phys, 512,
                       VRING_DESC_F_NEXT, 2); /* READABLE - wrong! */
    vring_raw_set_desc(&rxvr, 2, b2_phys, 512,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&rxvr, 0, 0);
    vring_raw_set_avail_idx(&rxvr, 1);

    return vv_kick_and_wait(dev, &rxvr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0039, VIRTIO_PCI_DEVICE_NET, test_net_rx_mixed_rw,
              "RX buffer chain with readable descriptor mixed in",
              VIRTIO_SPEC_V1_2, "5.1.6.4");
