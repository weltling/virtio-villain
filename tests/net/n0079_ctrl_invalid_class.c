/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0079: net_ctrl_invalid_class
 *
 * Send a control virtqueue command with an invalid class number.
 * Spec 5.1.6.5: The device MUST return VIRTIO_NET_ERR for
 * unrecognized class values rather than crashing.
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

static test_result_t test_net_ctrl_invalid_class(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct vring ctrl_vr;
    vring_alloc(&ctrl_vr, 64);
    vring_attach(dev, &ctrl_vr, 2);

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = 0xFF;   /* invalid class */
    hdr->command = 0x00;
    *data = 0;
    *ack = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t ack_phys = vv_virt_to_phys(ack);

    vring_raw_set_desc(&ctrl_vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&ctrl_vr, 1, data_phys, 1,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&ctrl_vr, 2, ack_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&ctrl_vr, 0, 0);
    vring_raw_set_avail_idx(&ctrl_vr, 1);

    (void)vr;
    return vv_kick_and_wait(dev, &ctrl_vr, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0079, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_invalid_class,
              "Control VQ: invalid class number",
              VIRTIO_SPEC_V1_2, "5.1.6.5");
