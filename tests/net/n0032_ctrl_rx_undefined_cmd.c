/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0032: net_ctrl_rx_undefined_cmd
 *
 * Send a control queue command with class=VIRTIO_NET_CTRL_RX but an
 * undefined command ID. Tests that the device rejects unknown commands
 * within a valid class without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_rx_bad_cmd(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_RX;
    hdr->command = 0xFF; /* undefined command within RX class */
    *ack = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t ack_phys = vv_virt_to_phys(ack);

    /* ctrl hdr (readable) -> ack (writable) */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, ack_phys, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0032, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_rx_bad_cmd,
              "CTRL_RX with undefined command ID (0xFF)",
              VIRTIO_SPEC_V1_2, "5.1.6.5.1", VV_QUEUE_LAST);
