/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0005: net_rx_writable_check
 *
 * Submit an RX buffer descriptor without the WRITE flag. The device
 * needs to write received frames into this buffer; without WRITE it
 * must reject rather than writing into a read-only descriptor.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_rx_writable(struct virtio_dev *dev,
                                          struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 1526);

    uint64_t buf_phys = vv_virt_to_phys(buf);

    /* RX buffer without WRITE flag - device cannot write into it */
    vring_raw_set_desc(vr, 0, buf_phys, 1526, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0005, VIRTIO_PCI_DEVICE_NET, test_net_rx_writable,
              "RX buffer descriptor without WRITE flag",
              VIRTIO_SPEC_V1_2, "5.1.6");
