/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0061: CTRL_ANNOUNCE ACK command (spec 5.1.6.5.5)
 *
 * Issue CTRL_ANNOUNCE with ACK command. Without the feature
 * being negotiated, the device should reject or ignore.
 * Tests that the device handles the announce protocol gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_NET_CTRL_ANNOUNCE     3
#define VIRTIO_NET_CTRL_ANNOUNCE_ACK 0

struct ctrl_hdr {
    uint8_t class;
    uint8_t command;
} __attribute__((packed));

static test_result_t test_ctrl_announce_ack(struct virtio_dev *dev,
                                            struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;
    if (nq < 3)
        return TEST_SKIP;

    uint16_t ctrl_q = nq - 1;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, ctrl_q);

    struct ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_ANNOUNCE;
    ctrl->command = VIRTIO_NET_CTRL_ANNOUNCE_ACK;
    *ack = 0xFF;

    /* ANNOUNCE has no data payload, just header + ack */
    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(ctrl), sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    return vv_kick_and_wait(dev, &cvr, ctrl_q, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0061, VIRTIO_PCI_DEVICE_NET, test_ctrl_announce_ack,
              "CTRL_ANNOUNCE ACK command processing",
              VIRTIO_SPEC_V1_2, "5.1.6.5.5");
