/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0018: Multiple chained descriptors in a single batch.
 *
 * Submit a chain of 4 writable descriptors as a single request.
 * The watchdog device expects a single flat descriptor per ping,
 * so chains should be either rejected or handled gracefully.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_watchdog_multi_chain(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    uint64_t phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, phys, 1,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, phys + 64, 1,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, phys + 128, 1,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, phys + 192, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(D0018, VIRTIO_PCI_DEVICE_WATCHDOG, test_watchdog_multi_chain,
              "Ping with chained descriptor batch",
              VIRTIO_SPEC_V1_2, "5.16");
