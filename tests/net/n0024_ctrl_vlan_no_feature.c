/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0024: net_ctrl_vlan_no_feature
 *
 * Issue a VIRTIO_NET_CTRL_VLAN_ADD command without having negotiated
 * VIRTIO_NET_F_CTRL_VLAN. Spec 5.1.6.5.5: driver MUST NOT issue
 * CTRL_VLAN commands without the feature.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_vlan_no_feature(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint16_t *vlan_id = (uint16_t *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_VLAN;
    ctrl->command = VIRTIO_NET_CTRL_VLAN_ADD;
    *vlan_id = 100; /* VLAN ID to add */
    *status = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* Header (readable) -> VLAN ID data (readable) -> status (writable) */
    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), 2,
                       VRING_DESC_F_NEXT, VV_QUEUE_LAST);
    vring_raw_set_desc(vr, 2, status_phys, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0024, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_vlan_no_feature,
              "CTRL_VLAN_ADD without CTRL_VLAN feature",
              VIRTIO_SPEC_V1_2, "5.1.6.5.5", VV_QUEUE_LAST);
