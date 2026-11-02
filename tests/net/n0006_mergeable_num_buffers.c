/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0006: net_mergeable_num_buffers
 *
 * Submit an RX buffer with a corrupted num_buffers field in the
 * mergeable receive buffer header area. The device must handle
 * this safely without reading beyond allocated buffers.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_mergeable_abuse(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /*
     * Provide a single small RX buffer. If the device writes
     * num_buffers = 0xFFFF into it (or reads a corrupted value),
     * it should not try to chain 65535 buffers.
     */
    struct virtio_net_hdr_mrg *buf = vv_alloc_pages(1);

    /* Pre-fill with garbage num_buffers to test device resilience */
    buf->num_buffers = 0xFFFF;

    /* Single RX buffer (writable) */
    vv_desc(vr, 0, buf, PAGE_SIZE, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(N0006, VIRTIO_PCI_DEVICE_NET, test_net_mergeable_abuse,
              "Corrupted num_buffers in mergeable RX header",
              VIRTIO_SPEC_V1_2, "5.1.6", 0);
