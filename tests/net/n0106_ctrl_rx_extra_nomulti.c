/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0106: net_ctrl_rx_extra_nomulti
 *
 * Send VIRTIO_NET_CTRL_RX with VIRTIO_NET_CTRL_RX_NOMULTI. When
 * VIRTIO_NET_F_CTRL_RX_EXTRA is active, the driver can filter
 * multicast traffic. Spec v1.3 5.1.6.5.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_rx_nomulti(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *on_off = (uint8_t *)ctrl + sizeof(*ctrl);
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_RX;
    ctrl->command = VIRTIO_NET_CTRL_RX_NOMULTI;
    *on_off = 1;
    *status = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), 1,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0106, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_rx_nomulti,
                "CTRL_RX NOMULTI via extended RX filter",
                VIRTIO_SPEC_V1_3, "5.1.6.5.1", VV_QUEUE_LAST);
