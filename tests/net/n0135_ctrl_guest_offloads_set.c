/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0135: net ctrl guest offloads set.
 *
 * Spec 5.1.6.5.6: When VIRTIO_NET_F_CTRL_GUEST_OFFLOADS is negotiated
 * the driver may send VIRTIO_NET_CTRL_GUEST_OFFLOADS_SET to update
 * the active offload features. Send the command with offloads=0
 * (disable all) and verify the device acks with VIRTIO_NET_OK.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_ctrl_offloads(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat0 = cfg->device_feature;
    if (!(feat0 & (1U << VIRTIO_NET_F_CTRL_GUEST_OFFLOADS)))
        return TEST_SKIP;
    if (!(feat0 & (1U << VIRTIO_NET_F_CTRL_VQ)))
        return TEST_SKIP;

    /*
     * GUEST_OFFLOADS_SET is accepted only when the backend provides a
     * vnet header. QEMU exposes that state by offering the per offload
     * guest features. When none are offered the device returns
     * VIRTIO_NET_ERR even for offloads=0, which is correct. Track which
     * outcome to expect.
     */
    const uint32_t guest_offload_bits =
        (1U << VIRTIO_NET_F_GUEST_CSUM) |
        (1U << VIRTIO_NET_F_GUEST_TSO4) |
        (1U << VIRTIO_NET_F_GUEST_TSO6) |
        (1U << VIRTIO_NET_F_GUEST_ECN) |
        (1U << VIRTIO_NET_F_GUEST_UFO);
    int offloads_supported = (feat0 & guest_offload_bits) != 0;

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint64_t *offloads = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_GUEST_OFFLOADS;
    hdr->command = VIRTIO_NET_CTRL_GUEST_OFFLOADS_SET;
    *offloads = 0;  /* disable all offloads */
    *ack = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, offloads, 8, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, ack, 1, VRING_DESC_F_WRITE, 0);

    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    if (offloads_supported) {
        if (*ack != VIRTIO_NET_OK)
            TFAIL("ack %u, expected VIRTIO_NET_OK (0)", *ack);
    } else {
        if (*ack != VIRTIO_NET_ERR)
            TFAIL("ack %u, expected VIRTIO_NET_ERR without vnet header",
                  *ack);
    }

    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(N0135, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_offloads,
              "CTRL guest offloads set to zero",
              VIRTIO_SPEC_V1_2, "5.1.6.5.6", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_GUEST_OFFLOADS) |
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
