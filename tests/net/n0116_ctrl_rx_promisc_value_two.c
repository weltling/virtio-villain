/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0116: CTRL_RX PROMISC with an out of range boolean value.
 *
 * Spec 5.1.6.5.1: the PROMISC payload is a single byte interpreted
 * as a boolean (0 or 1). Submit the command with value=2 and the
 * device must reject with VIRTIO_NET_ERR rather than silently
 * coercing the value or asserting.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_rx_promisc_two(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ) ||
        !virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_RX))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *onoff  = (uint8_t *)ctrl + sizeof(*ctrl);
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class   = VIRTIO_NET_CTRL_RX;
    ctrl->command = VIRTIO_NET_CTRL_RX_PROMISC;
    *onoff  = 2;        /* invalid: outside {0,1} */
    *status = 0xFF;

    uint64_t ctrl_phys   = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), 1,
                       VRING_DESC_F_NEXT, VV_QUEUE_LAST);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q_REQUIRES(N0116, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_rx_promisc_two,
                "CTRL_RX PROMISC with out of range boolean value",
                VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST,
                (1ULL << VIRTIO_NET_F_CTRL_VQ) |
                (1ULL << VIRTIO_NET_F_CTRL_RX), 0);
