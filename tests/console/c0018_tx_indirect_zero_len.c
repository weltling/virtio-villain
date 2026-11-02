/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0018: Console transmit indirect with zero length.
 *
 * Submit an indirect transmitq descriptor whose len field is 0,
 * implying zero entries in the indirect table. Spec 2.7.7 requires
 * len to be a non-zero multiple of 16. The device must reject the
 * request.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_tx_indirect_zero_len(struct virtio_dev *dev,
                                                       struct vring *vr)
{
    struct vring_desc *itab = vv_alloc_pages(1);

    vv_desc(vr, 0, itab, 0, VRING_DESC_F_INDIRECT, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(C0018, VIRTIO_PCI_DEVICE_CONSOLE,
                test_console_tx_indirect_zero_len,
                "Console transmit indirect with zero length",
                VIRTIO_SPEC_V1_2, "2.7.7", 1);
