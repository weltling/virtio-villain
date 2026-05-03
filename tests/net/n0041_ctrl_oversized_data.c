/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0041: net_ctrl_oversized_data
 *
 * Submit a control queue command with a data buffer much larger than
 * expected for the command type. Tests whether the device properly
 * bounds-checks the data length or blindly reads the full descriptor.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_NET_CTRL_RX       0
#define VIRTIO_NET_CTRL_RX_PROMISC 0

struct ctrl_hdr {
    uint8_t class;
    uint8_t command;
} __attribute__((packed));

static test_result_t test_net_ctrl_oversized_data(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    (void)vr;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;
    if (nq < 3) /* need at least RX + TX + ctrl */
        return TEST_SKIP;

    uint16_t ctrl_q = nq - 1;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, ctrl_q);

    struct ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1); /* 4096 bytes - way oversized */
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_RX;
    ctrl->command = VIRTIO_NET_CTRL_RX_PROMISC;
    /* PROMISC expects 1 byte of data (0 or 1). We send 4096 bytes. */
    memset(data, 0x01, 4096);
    *ack = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t ack_phys = vv_virt_to_phys(ack);

    vring_raw_set_desc(&cvr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, data_phys, 4096, /* oversized! */
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, ack_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    return vv_kick_and_wait(dev, &cvr, ctrl_q, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0041, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_oversized_data,
              "Control command with oversized data buffer (4096 bytes)",
              VIRTIO_SPEC_V1_2, "5.1.6.5");
