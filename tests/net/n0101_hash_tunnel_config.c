/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0101: net_hash_tunnel_report
 *
 * Send VIRTIO_NET_CTRL_MQ_HASH_CONFIG to configure hashing over
 * inner (tunnel) headers. Spec v1.3 5.1.6.5.4: when
 * VIRTIO_NET_F_HASH_TUNNEL is negotiated, the device can hash on
 * inner packet headers for tunneled traffic.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_net_ctrl_hdr {
    uint8_t class;
    uint8_t command;
} __attribute__((packed));

#define VIRTIO_NET_CTRL_MQ             4
#define VIRTIO_NET_CTRL_MQ_HASH_CONFIG 4

struct virtio_net_hash_config {
    uint32_t hash_types;
    uint16_t indirection_table_mask;
    uint16_t unclassified_queue;
    uint8_t  hash_key_length;
    uint8_t  hash_key[40];
} __attribute__((packed));

static test_result_t test_net_hash_tunnel(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_hash_config *hcfg =
        (struct virtio_net_hash_config *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_HASH_CONFIG;
    hcfg->hash_types = 0x01;  /* IPv4 */
    hcfg->indirection_table_mask = 0x0F;
    hcfg->unclassified_queue = 0;
    hcfg->hash_key_length = 40;
    memset(hcfg->hash_key, 0x6D, 40);
    *status = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), sizeof(*hcfg),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0101, VIRTIO_PCI_DEVICE_NET, test_net_hash_tunnel,
                "Hash config for tunnel inner headers",
                VIRTIO_SPEC_V1_3, "5.1.6.5.4", VV_QUEUE_LAST);
