/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0190: rx_self_loop
 *
 * A receive descriptor sets VRING_DESC_F_NEXT with its next field
 * pointing back at itself. The device must bound its walk of the
 * chain rather than loop forever.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_rx_self_loop(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 1526,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0190, VIRTIO_PCI_DEVICE_NET, test_net_rx_self_loop,
                "Receive descriptor chained to itself",
                VIRTIO_SPEC_V1_2, "2.7.5", 0);
