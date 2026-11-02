/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0109: net_ctrl_mac_table_oversized
 *
 * Submit a MAC table via CTRL_MAC whose entry count exceeds the
 * descriptor length. Spec 5.1.6.5.3 says the device must validate
 * MAC table boundaries. A truncated table must not cause an OOB
 * read in the device model.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_mac_table_oversized(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_RX))
        return TEST_SKIP;

    uint8_t *page = vv_alloc_pages(1);

    struct virtio_net_ctrl_hdr *ctrl = (void *)page;
    ctrl->class = VIRTIO_NET_CTRL_MAC;
    ctrl->command = VIRTIO_NET_CTRL_MAC_TABLE_SET;

    /* Unicast table: claim 1000 entries but only provide header */
    uint32_t *uc_count = (uint32_t *)(page + sizeof(*ctrl));
    *uc_count = 1000;

    /* Multicast table: zero entries */
    uint32_t *mc_count = (uint32_t *)(page + sizeof(*ctrl) + 4);
    *mc_count = 0;

    uint8_t *status = vv_alloc_pages(1);
    *status = 0xFF;

    /* Header + unicast table (truncated) */
    vv_desc(vr, 0, page, sizeof(*ctrl) + 4 + 4, VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q_REQUIRES(N0109, VIRTIO_PCI_DEVICE_NET, test_net_mac_table_oversized,
                "CTRL_MAC table entry count exceeds descriptor",
                VIRTIO_SPEC_V1_2, "5.1.6.5.3", VV_QUEUE_LAST,
                (1ULL << VIRTIO_NET_F_CTRL_VQ) |
                (1ULL << VIRTIO_NET_F_CTRL_RX), 0);
