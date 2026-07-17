/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0011: net_ctrl_rx_no_feature
 *
 * Issue VIRTIO_NET_CTRL_RX commands without negotiating the
 * VIRTIO_NET_F_CTRL_RX feature. Spec 5.1.6.5.1.2: driver MUST NOT
 * issue CTRL_RX commands without the feature.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_rx_no_feature(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_NET_F_CTRL_VQ))
        return TEST_SKIP;

    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *onoff = (uint8_t *)ctrl + sizeof(*ctrl);
    uint8_t *status = vv_alloc_pages(1);

    /*
     * Format a control command: class=CTRL_RX, cmd=PROMISC, data=1
     * submitted without ever negotiating VIRTIO_NET_F_CTRL_VQ or
     * VIRTIO_NET_F_CTRL_RX features (harness negotiates zero features).
     */
    ctrl->class = VIRTIO_NET_CTRL_RX;
    ctrl->command = VIRTIO_NET_CTRL_RX_PROMISC;
    *onoff = 1;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* descriptor 0: header (device-readable) */
    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    /* descriptor 1: on/off byte (device-readable) */
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), 1,
                       VRING_DESC_F_NEXT, 2);
    /* descriptor 2: status byte (device-writable) */
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q_REQUIRES(N0011, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_rx_no_feature,
              "CTRL_RX command without CTRL_RX feature",
              VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST,
              (1ULL << VIRTIO_NET_F_CTRL_VQ), 0);
