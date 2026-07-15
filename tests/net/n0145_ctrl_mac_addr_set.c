/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0145: ctrl MAC address set valid.
 *
 * Spec 5.1.6.5.2: When VIRTIO_NET_F_CTRL_MAC_ADDR is negotiated the
 * driver may change the device MAC via CTRL_MAC_ADDR_SET. Send a
 * valid unicast MAC and verify the device responds VIRTIO_NET_OK.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_mac_set(struct virtio_dev *dev,
                                           struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_CTRL_MAC_ADDR)))
        return TEST_SKIP;
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_CTRL_VQ)))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *mac = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_MAC;
    hdr->command = VIRTIO_NET_CTRL_MAC_ADDR_SET;
    /* Valid unicast MAC: locally administered, not multicast */
    mac[0] = 0x02; mac[1] = 0xDE; mac[2] = 0xAD;
    mac[3] = 0xBE; mac[4] = 0xEF; mac[5] = 0x01;
    *ack = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(mac), 6,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    if (*ack != VIRTIO_NET_OK)
        TFAIL("ack %u, expected VIRTIO_NET_OK", *ack);

    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(N0145, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_mac_set,
              "CTRL MAC address set valid unicast",
              VIRTIO_SPEC_V1_2, "5.1.6.5.2", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_MAC_ADDR) |
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
