/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0076: net_ctrl_vlan_add
 *
 * Send VIRTIO_NET_CTRL_VLAN ADD command to add a VLAN to the filter.
 * Spec 5.1.6.5.3: If VIRTIO_NET_F_CTRL_VLAN is negotiated, the
 * driver can filter by VLAN ID.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_vlan_add(struct virtio_dev *dev,
                                            struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VLAN))
        return TEST_SKIP;

    struct vring ctrl_vr;
    vring_alloc(&ctrl_vr, 64);
    vring_attach(dev, &ctrl_vr, 2);

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint16_t *vlan_id = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_VLAN;
    hdr->command = VIRTIO_NET_CTRL_VLAN_ADD;
    *vlan_id = 100;  /* VLAN ID 100 */
    *ack = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t vlan_phys = vv_virt_to_phys(vlan_id);
    uint64_t ack_phys = vv_virt_to_phys(ack);

    vring_raw_set_desc(&ctrl_vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&ctrl_vr, 1, vlan_phys, sizeof(*vlan_id),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&ctrl_vr, 2, ack_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&ctrl_vr, 0, 0);
    vring_raw_set_avail_idx(&ctrl_vr, 1);

    (void)vr;
    return vv_kick_and_wait(dev, &ctrl_vr, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(N0076, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_vlan_add,
              "Control VQ: add VLAN filter",
              VIRTIO_SPEC_V1_2, "5.1.6.5.3",
              (1ULL << VIRTIO_NET_F_CTRL_VQ) |
              (1ULL << VIRTIO_NET_F_CTRL_VLAN), 0);
