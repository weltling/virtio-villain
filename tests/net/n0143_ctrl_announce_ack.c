/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0143: ctrl ANNOUNCE ack via control virtqueue.
 *
 * Spec 5.1.6.5.4: When VIRTIO_NET_F_GUEST_ANNOUNCE is negotiated
 * the driver sends CTRL_ANNOUNCE_ACK after it has completed a
 * gratuitous ARP burst. The device must consume the command.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_announce(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_GUEST_ANNOUNCE)))
        return TEST_SKIP;
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_CTRL_VQ)))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_ANNOUNCE;
    hdr->command = VIRTIO_NET_CTRL_ANNOUNCE_ACK;
    *ack = 0xFF;

    /*
     * The ack byte carries a defined status, VIRTIO_NET_OK or
     * VIRTIO_NET_ERR. While an announce is pending, that is
     * VIRTIO_NET_S_ANNOUNCE set in the config status, the device must
     * ack OK. With no pending announce a VMM may ack OK or ERR: CH
     * acks OK, QEMU acks ERR, both consume the command. Sample the
     * bit before the ack, since the ack clears it.
     */
    int announce_pending = 0;
    if (dev->device_cfg &&
        dev->device_cfg_length >= VIRTIO_NET_CFG_STATUS_OFFSET + 2) {
        volatile uint16_t *status = (volatile uint16_t *)(
            (uint8_t *)dev->device_cfg + VIRTIO_NET_CFG_STATUS_OFFSET);
        __sync_synchronize();
        announce_pending = (*status & VIRTIO_NET_S_ANNOUNCE) != 0;
    }

    /* ANNOUNCE_ACK has no data payload, just hdr + ack */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    if (*ack != VIRTIO_NET_OK && *ack != VIRTIO_NET_ERR)
        TFAIL("ack 0x%02x, expected VIRTIO_NET_OK or VIRTIO_NET_ERR",
              *ack);
    if (announce_pending && *ack != VIRTIO_NET_OK)
        TFAIL("ack %u, expected VIRTIO_NET_OK for a pending announce",
              *ack);

    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(N0143, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_announce,
              "CTRL ANNOUNCE ACK",
              VIRTIO_SPEC_V1_2, "5.1.6.5.4", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_GUEST_ANNOUNCE) |
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
