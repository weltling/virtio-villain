/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0018: Multiple chained descriptors in a single batch.
 *
 * Submit a chain of 4 writable descriptors as a single request.
 * The watchdog device expects a single flat descriptor per ping,
 * so chains should be either rejected or handled gracefully.
 */
#include "lib/virtio_test.h"

static test_result_t test_watchdog_multi_chain(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);

    uint64_t phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, phys, 1,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, phys + 64, 1,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, phys + 128, 1,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, phys + 192, 1,
                       VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(D0018, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_multi_chain,
              "Ping with chained descriptor batch",
              VIRTIO_SPEC_V1_2, "-");
