/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0071: net_link_status_transition
 *
 * Read VIRTIO_NET_S_LINK_UP from config, then submit TX.
 * Tests that device behavior is consistent with link status -
 * TX should succeed if link is up.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_net_hdr {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __attribute__((packed));

struct virtio_net_config {
    uint8_t  mac[6];
    uint16_t status;
} __attribute__((packed));

#define VIRTIO_NET_S_LINK_UP 1

static test_result_t test_net_link_status(struct virtio_dev *dev,
                                          struct vring *vr)
{
    (void)vr;

    /* Read link status from device config */
    volatile struct virtio_net_config *netcfg =
        (volatile struct virtio_net_config *)dev->device_cfg;

    __sync_synchronize();
    uint16_t status = netcfg->status;

    if (!(status & VIRTIO_NET_S_LINK_UP))
        return TEST_SKIP;

    /* Link is up, submit a TX packet */
    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    memset(hdr, 0, sizeof(*hdr));
    memset(frame, 0xFF, 6);       /* dst broadcast */
    memset(frame + 6, 0x02, 6);   /* src */
    frame[12] = 0x08; frame[13] = 0x00;
    memset(frame + 14, 0x77, 46); /* payload */

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&txvr, 1, vv_virt_to_phys(frame), 60, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    return vv_kick_and_wait(dev, &txvr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0071, VIRTIO_PCI_DEVICE_NET, test_net_link_status,
              "TX after verifying link status is up",
              VIRTIO_SPEC_V1_2, "5.1.6");
