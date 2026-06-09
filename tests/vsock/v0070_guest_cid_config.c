/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0070: guest_cid in the device config is non zero.
 *
 * Spec 5.10.4: vsock device config exposes a 64 bit guest_cid
 * which MUST be a value other than 0 (zero is reserved for
 * hypervisor and 1 for loopback). Read the device config and
 * verify guest_cid is in the legal driver range.
 */
#include "tests/test.h"

static test_result_t test(struct virtio_dev *dev, struct vring *vr)
{
    (void)vr;
    if (!dev->device_cfg || dev->device_cfg_length < 8)
        return TEST_SKIP;
    volatile uint64_t *gcid = dev->device_cfg;
    uint64_t v = *gcid;
    if (v == 0)
        TFAIL("guest_cid is 0 (reserved for hypervisor)");
    if (v == 0xFFFFFFFFFFFFFFFFULL)
        TFAIL("guest_cid is reserved sentinel 0xffffffffffffffff");
    return TEST_PASS;
}

REGISTER_TEST(V0070, VIRTIO_PCI_DEVICE_VSOCK, test,
              "guest_cid in vsock config is a legal value",
              VIRTIO_SPEC_V1_4, "5.10.4");
