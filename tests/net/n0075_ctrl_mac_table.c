/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0075: net_ctrl_mac_table_set
 *
 * Send VIRTIO_NET_CTRL_MAC MAC_TABLE_SET command with unicast and
 * multicast MAC addresses.
 * Spec 5.1.6.5.2: The driver sets the MAC filtering tables via the
 * control virtqueue.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_mac_table_set(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_RX))
        return TEST_SKIP;

    struct vring ctrl_vr;
    vring_alloc(&ctrl_vr, 64);
    vring_attach(dev, &ctrl_vr, 2);

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_MAC;
    hdr->command = VIRTIO_NET_CTRL_MAC_TABLE_SET;

    /* Build MAC table: 1 unicast entry + 1 multicast entry */
    struct virtio_net_ctrl_mac *uc = (void *)payload;
    uc->entries = 1;
    /* Unicast: 02:00:00:00:00:01 */
    uc->macs[0][0] = 0x02; uc->macs[0][1] = 0x00;
    uc->macs[0][2] = 0x00; uc->macs[0][3] = 0x00;
    uc->macs[0][4] = 0x00; uc->macs[0][5] = 0x01;

    uint32_t uc_size = 4 + 6;  /* entries field + 1 MAC */

    struct virtio_net_ctrl_mac *mc = (void *)(payload + uc_size);
    mc->entries = 1;
    /* Multicast: 01:00:5e:00:00:01 */
    mc->macs[0][0] = 0x01; mc->macs[0][1] = 0x00;
    mc->macs[0][2] = 0x5e; mc->macs[0][3] = 0x00;
    mc->macs[0][4] = 0x00; mc->macs[0][5] = 0x01;

    uint32_t mc_size = 4 + 6;
    uint32_t total_payload = uc_size + mc_size;

    *ack = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t payload_phys = vv_virt_to_phys(payload);
    uint64_t ack_phys = vv_virt_to_phys(ack);

    vring_raw_set_desc(&ctrl_vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&ctrl_vr, 1, payload_phys, total_payload,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&ctrl_vr, 2, ack_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&ctrl_vr, 0, 0);
    vring_raw_set_avail_idx(&ctrl_vr, 1);

    (void)vr;
    return vv_kick_and_wait(dev, &ctrl_vr, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(N0075, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_mac_table_set,
              "Control VQ: set MAC filtering table",
              VIRTIO_SPEC_V1_2, "5.1.6.5.2",
              (1ULL << VIRTIO_NET_F_CTRL_VQ) |
              (1ULL << VIRTIO_NET_F_CTRL_RX), 0);
