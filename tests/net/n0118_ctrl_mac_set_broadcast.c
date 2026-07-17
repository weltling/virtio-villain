/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0118: CTRL_MAC_ADDR_SET to the broadcast address.
 *
 * Spec 5.1.6.5.1: CTRL_MAC_ADDR_SET installs a new station MAC.
 * Submit a CTRL command with the broadcast address
 * FF:FF:FF:FF:FF:FF as the new MAC. A broadcast source MAC is
 * never valid for a real station. The device must reject the
 * command rather than installing a broadcast as its station
 * address.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_mac_set_broadcast(struct virtio_dev *dev,
                                                struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_MAC_ADDR))
        return TEST_SKIP;

    (void)vr;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, VV_QUEUE_LAST);

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *mac = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_MAC;
    hdr->command = VIRTIO_NET_CTRL_MAC_ADDR_SET;
    memset(mac, 0xFF, 6);
    *ack = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(mac), 6,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    return vv_kick_and_wait(dev, &cvr, VV_QUEUE_LAST, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(N0118, VIRTIO_PCI_DEVICE_NET, test_net_mac_set_broadcast,
              "CTRL_MAC_ADDR_SET with broadcast MAC",
              VIRTIO_SPEC_V1_2, "5.1.6.5.1",
              (1ULL << VIRTIO_NET_F_CTRL_VQ) |
              (1ULL << VIRTIO_NET_F_CTRL_MAC_ADDR), 0);
