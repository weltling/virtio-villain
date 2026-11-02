/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0012: FUSE request with truncated header.
 *
 * Spec 5.11.6: The readable part of the descriptor must contain
 * at least a fuse_in_header (40 bytes). Provide only 4 bytes.
 * The device must detect the short buffer and not read beyond.
 */
#include "lib/virtio_test.h"

static test_result_t test_fs_truncated_header(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);

    /* Only provide 4 bytes of readable data */
    uint64_t phys = vv_virt_to_phys(page);

    vring_raw_set_desc(vr, 0, phys, 4, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, phys + 256, 256,
                       VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(F0012, VIRTIO_PCI_DEVICE_FS, test_fs_truncated_header,
                "FUSE request with truncated header",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
