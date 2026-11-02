/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0019: Console transmit indirect with non-multiple-of-16 length.
 *
 * Submit an indirect transmitq descriptor whose len is 17, which
 * is not a multiple of sizeof(struct vring_desc)=16. Spec 2.7.7
 * requires len to be a multiple of 16. The device must reject.
 */
#include "lib/virtio_test.h"

static test_result_t test_console_tx_indirect_unaligned(struct virtio_dev *dev,
                                                        struct vring *vr)
{
    struct vring_desc *itab = vv_alloc_pages(1);

    vv_desc(vr, 0, itab, 17, VRING_DESC_F_INDIRECT, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(C0019, VIRTIO_PCI_DEVICE_CONSOLE,
                test_console_tx_indirect_unaligned,
                "Console transmit indirect with unaligned length",
                VIRTIO_SPEC_V1_2, "2.7.7", 1);
