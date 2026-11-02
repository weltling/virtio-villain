/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0184: tx_bad_addr
 *
 * A transmit chain has its frame descriptor point at an address far
 * outside guest RAM. The device must handle the invalid source
 * address gracefully rather than fault the host.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_bad_addr(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xdeadbeefdead0000ULL, 1514, 0, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0184, VIRTIO_PCI_DEVICE_NET, test_net_tx_bad_addr,
                "Transmit frame descriptor with a wild address",
                VIRTIO_SPEC_V1_2, "2.7.5", 1);
