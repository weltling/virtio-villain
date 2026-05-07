/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0082: announce ack clears the announce bit
 *
 * Spec 5.1.6.6 says when VIRTIO_NET_S_ANNOUNCE is set in the
 * status config field the driver sends VIRTIO_NET_CTRL_ANNOUNCE
 * with command VIRTIO_NET_CTRL_ANNOUNCE_ACK and the device clears
 * the bit in response. Without an external trigger the bit is not
 * normally set, so this test issues the ACK unsolicited and
 * verifies the device handles a redundant ack without wedging
 * and without setting the bit afterwards.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

#define VIRTIO_NET_F_STATUS 16
#define VIRTIO_NET_F_GUEST_ANNOUNCE 21
#define VIRTIO_NET_CFG_STATUS_OFFSET 6
#define VIRTIO_NET_S_ANNOUNCE 2

#define VIRTIO_NET_CTRL_ANNOUNCE 3
#define VIRTIO_NET_CTRL_ANNOUNCE_ACK 0

struct virtio_net_ctrl_hdr {
    uint8_t class;
    uint8_t command;
} __attribute__((packed));

static test_result_t test_net_announce_ack_clears(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_STATUS)))
        return TEST_SKIP;
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_GUEST_ANNOUNCE)))
        return TEST_SKIP;

    if (!dev->device_cfg ||
        dev->device_cfg_length < VIRTIO_NET_CFG_STATUS_OFFSET + 2)
        return TEST_SKIP;

    struct vring ctrl_vr;
    vring_alloc(&ctrl_vr, 64);
    /* control queue is the last queue */
    vring_attach(dev, &ctrl_vr, (uint16_t)(cfg->num_queues - 1));

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_ANNOUNCE;
    hdr->command = VIRTIO_NET_CTRL_ANNOUNCE_ACK;
    *ack = 0xFF;

    vring_raw_set_desc(&ctrl_vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&ctrl_vr, 1, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&ctrl_vr, 0, 0);
    vring_raw_set_avail_idx(&ctrl_vr, 1);

    test_result_t r = vv_kick_and_wait(dev, &ctrl_vr,
                                       cfg->num_queues - 1, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* After ACK the status announce bit must be clear */
    volatile uint16_t *status = (volatile uint16_t *)(
        (uint8_t *)dev->device_cfg + VIRTIO_NET_CFG_STATUS_OFFSET);
    __sync_synchronize();
    if (*status & VIRTIO_NET_S_ANNOUNCE)
        TFAIL("*status & VIRTIO_NET_S_ANNOUNCE");

    return TEST_PASS;
}

REGISTER_TEST(N0082, VIRTIO_PCI_DEVICE_NET, test_net_announce_ack_clears,
              "ANNOUNCE_ACK leaves announce bit clear",
              VIRTIO_SPEC_V1_2, "5.1.6.6");
