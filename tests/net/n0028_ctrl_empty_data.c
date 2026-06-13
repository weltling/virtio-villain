/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0028: net_ctrl_empty_data
 *
 * Issue a control queue command with only the header and ack byte but
 * no data buffer between them. Some commands require a data payload;
 * the device must handle a missing/empty data gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_empty_data(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    /*
     * CTRL_RX PROMISC normally expects a 1-byte on/off data payload.
     * We skip it entirely - just header + ack.
     */
    ctrl->class = VIRTIO_NET_CTRL_RX;
    ctrl->command = VIRTIO_NET_CTRL_RX_PROMISC;
    *status = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* descriptor 0: header only (device-readable) */
    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl),
                       VRING_DESC_F_NEXT, 1);
    /* descriptor 1: ack byte (device-writable) - no data in between */
    vring_raw_set_desc(vr, 1, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0028, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_empty_data,
              "Control command with no data payload",
              VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST);
