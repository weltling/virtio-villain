/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0111: net_ctrl_announce_without_status
 *
 * Send CTRL_ANNOUNCE ACK without VIRTIO_NET_F_GUEST_ANNOUNCE.
 * Spec 5.1.6.5.2 says announcement requires the feature bit.
 * The device must reject or ignore the command when the feature
 * was not negotiated.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_announce_no_feature(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    memset(page, 0, 4096);

    struct virtio_net_ctrl_hdr *ctrl = (void *)page;
    ctrl->class = VIRTIO_NET_CTRL_ANNOUNCE;
    ctrl->command = VIRTIO_NET_CTRL_ANNOUNCE_ACK;

    uint8_t *status = vv_alloc_pages(1);
    *status = 0xFF;

    uint64_t page_phys = vv_virt_to_phys(page);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, page_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, VV_QUEUE_LAST, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0111, VIRTIO_PCI_DEVICE_NET, test_net_announce_no_feature,
                "CTRL_ANNOUNCE without guest announce feature",
                VIRTIO_SPEC_V1_2, "5.1.6.5.2", VV_QUEUE_LAST);
