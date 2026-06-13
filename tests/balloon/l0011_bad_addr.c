/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0011: Balloon inflate with bad addr.
 *
 * Descriptor addr points to 0xFFFFFFFFFFFF0000, outside any
 * mapped guest memory. The device must refuse to dereference it.
 *
 * Spec 5.5.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_balloon_bad_addr(struct virtio_dev *dev,
                                           struct vring *vr)
{
    vring_raw_set_desc(vr, 0, 0xFFFFFFFFFFFF0000ULL, 16, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(L0011, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_bad_addr,
              "Inflate with out-of-range address",
              VIRTIO_SPEC_V1_2, "5.5.6.1");
