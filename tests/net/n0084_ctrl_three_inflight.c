/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0084: three ctrl vq commands in flight
 *
 * Spec 5.1.6 allows the driver to enqueue multiple ctrl commands
 * before kicking. Place three back to back commands of different
 * classes (RX MODE, MAC MODE, then a no op MAC TABLE SET with
 * empty tables) into the ctrl ring, kick once, and wait until the
 * used index advances by three. A VMM that only processes one ctrl
 * command per kick will time out.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_three_inflight(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t off = cfg->device_feature;
    if (!(off & (1U << VIRTIO_NET_F_CTRL_VQ)))
        return TEST_SKIP;

    struct vring ctrl_vr;
    vring_alloc(&ctrl_vr, 64);
    vring_attach(dev, &ctrl_vr, (uint16_t)(cfg->num_queues - 1));

    struct virtio_net_ctrl_hdr *h0 = vv_alloc_pages(1);
    struct virtio_net_ctrl_hdr *h1 = vv_alloc_pages(1);
    struct virtio_net_ctrl_hdr *h2 = vv_alloc_pages(1);
    uint8_t *promisc = vv_alloc_pages(1);
    uint8_t *mac = vv_alloc_pages(1);
    struct mac_table_zero *uni = vv_alloc_pages(1);
    struct mac_table_zero *multi = vv_alloc_pages(1);
    uint8_t *a0 = vv_alloc_pages(1);
    uint8_t *a1 = vv_alloc_pages(1);
    uint8_t *a2 = vv_alloc_pages(1);

    h0->class = VIRTIO_NET_CTRL_RX;
    h0->command = VIRTIO_NET_CTRL_RX_PROMISC;
    *promisc = 0;

    h1->class = VIRTIO_NET_CTRL_MAC;
    h1->command = VIRTIO_NET_CTRL_MAC_ADDR_SET;
    static const uint8_t newmac[6] = { 0x02, 0xaa, 0xbb, 0xcc, 0xdd, 0xee };
    memcpy(mac, newmac, 6);

    h2->class = VIRTIO_NET_CTRL_MAC;
    h2->command = VIRTIO_NET_CTRL_MAC_TABLE_SET;
    uni->entries = 0;
    multi->entries = 0;

    *a0 = 0xFF;
    *a1 = 0xFF;
    *a2 = 0xFF;

    /* Chain 1: idx 0->1->2 */
    vring_raw_set_desc(&ctrl_vr, 0, vv_virt_to_phys(h0), sizeof(*h0),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&ctrl_vr, 1, vv_virt_to_phys(promisc), 1,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&ctrl_vr, 2, vv_virt_to_phys(a0), 1,
                       VRING_DESC_F_WRITE, 0);

    /* Chain 2 head 3, MAC ADDR SET with mac payload */
    vring_raw_set_desc(&ctrl_vr, 3, vv_virt_to_phys(h1), sizeof(*h1),
                       VRING_DESC_F_NEXT, 4);
    vring_raw_set_desc(&ctrl_vr, 4, vv_virt_to_phys(mac), 6,
                       VRING_DESC_F_NEXT, 5);
    vring_raw_set_desc(&ctrl_vr, 5, vv_virt_to_phys(a1), 1,
                       VRING_DESC_F_WRITE, 0);

    /* Chain 3 head 6, MAC TABLE SET with two zero count tables */
    vring_raw_set_desc(&ctrl_vr, 6, vv_virt_to_phys(h2), sizeof(*h2),
                       VRING_DESC_F_NEXT, 7);
    vring_raw_set_desc(&ctrl_vr, 7, vv_virt_to_phys(uni), sizeof(*uni),
                       VRING_DESC_F_NEXT, 8);
    vring_raw_set_desc(&ctrl_vr, 8, vv_virt_to_phys(multi), sizeof(*multi),
                       VRING_DESC_F_NEXT, 9);
    vring_raw_set_desc(&ctrl_vr, 9, vv_virt_to_phys(a2), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&ctrl_vr, 0, 0);
    vring_raw_set_avail(&ctrl_vr, 1, 3);
    vring_raw_set_avail(&ctrl_vr, 2, 6);
    vring_raw_set_avail_idx(&ctrl_vr, 3);

    __sync_synchronize();
    virtio_pci_kick(dev, ctrl_vr.queue);

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (ctrl_vr.used->idx >= 3)
            return TEST_PASS;
        elapsed += 10000;
    }

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(N0084, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_three_inflight,
              "Three ctrl vq commands in flight on a single kick",
              VIRTIO_SPEC_V1_2, "5.1.6");
