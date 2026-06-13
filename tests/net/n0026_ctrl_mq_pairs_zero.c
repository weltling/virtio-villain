/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0026: net_ctrl_mq_pairs_zero
 *
 * Issue VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET with virtqueue_pairs=0.
 * Spec 5.1.6.5.6.1: the device expects a valid number of queue pairs
 * (1 <= pairs <= max). Zero is invalid and must not crash the device.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_mq_zero(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint16_t *pairs = (uint16_t *)((uint8_t *)ctrl + sizeof(*ctrl));
    uint8_t *status = vv_alloc_pages(1);

    ctrl->class = VIRTIO_NET_CTRL_MQ;
    ctrl->command = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
    *pairs = 0; /* invalid: zero pairs */
    *status = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* descriptor 0: header (device-readable) */
    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    /* descriptor 1: pairs value (device-readable) */
    vring_raw_set_desc(vr, 1, ctrl_phys + sizeof(*ctrl), 2,
                       VRING_DESC_F_NEXT, VV_QUEUE_LAST);
    /* descriptor 2: status byte (device-writable) */
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0026, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_mq_zero,
              "CTRL_MQ VQ_PAIRS_SET with pairs=0",
              VIRTIO_SPEC_V1_2, "5.1.6.5.6", VV_QUEUE_LAST);
