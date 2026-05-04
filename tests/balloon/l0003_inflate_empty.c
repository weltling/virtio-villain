/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0003: Balloon inflate with zero PFNs (empty descriptor).
 *
 * Submit an inflate request with zero-length data. The device
 * must handle this gracefully.
 *
 * Spec 5.5.6.1: Robustness test.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_balloon_inflate_empty(struct virtio_dev *dev,
                                                struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    /* Zero-length descriptor */
    vring_raw_set_desc(vr, 0, buf_phys, 0, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0003, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_inflate_empty,
              "Inflate with zero-length PFN array",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
