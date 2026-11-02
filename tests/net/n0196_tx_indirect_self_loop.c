/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0196: tx_indirect_self_loop
 *
 * With VIRTIO_F_INDIRECT_DESC negotiated the first entry of a transmit
 * indirect table chains its next field to itself. The device must
 * bound its walk of the table rather than loop forever.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_tx_indirect_self_loop(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_INDIRECT_DESC))
        return TEST_SKIP;

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    struct vring_desc *indirect = vv_alloc_pages(1);

    indirect[0].addr = vv_virt_to_phys(hdr);
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 0;      /* points at itself */

    vv_desc(vr, 0, indirect, sizeof(struct vring_desc), VRING_DESC_F_INDIRECT,
            0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q_REQUIRES(N0196, VIRTIO_PCI_DEVICE_NET,
                         test_net_tx_indirect_self_loop,
                         "Transmit indirect entry chained to itself",
                         VIRTIO_SPEC_V1_2, "2.7.5.3", 1,
                         (1ULL << VIRTIO_F_INDIRECT_DESC), 0);
