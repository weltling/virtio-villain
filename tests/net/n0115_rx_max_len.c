/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0115: Net RX with maximum length descriptor.
 *
 * Spec 5.1.6: Post a receive buffer with len = 0xFFFFFFFF.
 * The device must not overflow internal calculations when
 * confronted with a maximum size writable descriptor.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_rx_max_len(struct virtio_dev *dev,
                                         struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 0xFFFFFFFFU,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0115, VIRTIO_PCI_DEVICE_NET, test_net_rx_max_len,
              "Net RX buffer with maximum descriptor length",
              VIRTIO_SPEC_V1_2, "5.1.6");
