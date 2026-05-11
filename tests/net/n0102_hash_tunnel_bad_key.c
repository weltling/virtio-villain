/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0102: net_hash_tunnel_bad_key_len
 *
 * Send hash config with hash_key_length set to 0. The device
 * should reject or handle gracefully per spec v1.3 5.1.6.5.4.
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

struct virtio_net_hash_config_short {
    uint32_t hash_types;
    uint16_t indirection_table_mask;
    uint16_t unclassified_queue;
    uint8_t  hash_key_length;
    /* No key bytes since length is 0 */
} __attribute__((packed));

static test_result_t test_net_hash_bad_key(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    struct virtio_net_hash_config_short *hcfg =
        (struct virtio_net_hash_config_short *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_HASH_CONFIG;
    hcfg->hash_types = 0x01;
    hcfg->indirection_table_mask = 0x0F;
    hcfg->unclassified_queue = 0;
    hcfg->hash_key_length = 0;  /* Invalid: zero length key */
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

REGISTER_TEST_Q(N0102, VIRTIO_PCI_DEVICE_NET, test_net_hash_bad_key,
                "Hash config with zero length hash key",
                VIRTIO_SPEC_V1_3, "5.1.6.5.4", VV_QUEUE_LAST);
