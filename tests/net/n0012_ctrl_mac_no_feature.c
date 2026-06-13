/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0012: net_ctrl_mac_no_feature
 *
 * Issue VIRTIO_NET_CTRL_MAC commands without negotiating the
 * VIRTIO_NET_F_CTRL_MAC_ADDR feature. Spec 5.1.6.5.2.2: driver
 * MUST NOT issue CTRL_MAC class commands without the feature.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_mac_no_feature(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *mac_data = (uint8_t *)ctrl + sizeof(*ctrl);
    uint8_t *status = vv_alloc_pages(1);

    /* MAC address set command without negotiating feature */
    ctrl->class = VIRTIO_NET_CTRL_MAC;
    ctrl->command = VIRTIO_NET_CTRL_MAC_ADDR_SET;
    /* 6-byte MAC address */
    mac_data[0] = 0x02;
    mac_data[1] = 0xDE;
    mac_data[2] = 0xAD;
    mac_data[3] = 0xBE;
    mac_data[4] = 0xEF;
    mac_data[5] = 0x01;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), 6,
                       VRING_DESC_F_NEXT, VV_QUEUE_LAST);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0012, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_mac_no_feature,
              "CTRL_MAC command without MAC feature",
              VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST);
