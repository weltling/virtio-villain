/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0060: CTRL_GUEST_OFFLOADS without feature (spec 5.1.6.5.6)
 *
 * Issue a CTRL_GUEST_OFFLOADS_SET command without negotiating
 * VIRTIO_NET_F_CTRL_GUEST_OFFLOADS.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_ctrl_guest_offloads(struct virtio_dev *dev,
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

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint64_t *offloads = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_GUEST_OFFLOADS;
    ctrl->command = VIRTIO_NET_CTRL_GUEST_OFFLOADS_SET;
    *offloads = 0xFFFFFFFFFFFFFFFFULL; /* all offloads */
    *ack = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(ctrl), sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(offloads), sizeof(*offloads),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    return vv_kick_and_wait(dev, &cvr, ctrl_q, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0060, VIRTIO_PCI_DEVICE_NET, test_ctrl_guest_offloads,
              "CTRL_GUEST_OFFLOADS_SET without feature negotiated",
              VIRTIO_SPEC_V1_2, "5.1.6.5.6");
