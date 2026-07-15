/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0078: read guest_cid from vsock device config.
 *
 * Spec 5.10.4: The device config contains a 64-bit guest_cid at
 * offset 0. It must not be 0 (reserved for hypervisor), 1 (reserved
 * for host), or 0xFFFFFFFF (VMADDR_CID_ANY). Read it and validate.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_vsock_config_cid(struct virtio_dev *dev,
                                           struct vring *vr)
{
    (void)vr;

    if (!dev->device_cfg || dev->device_cfg_length < 8)
        return TEST_SKIP;

    volatile uint64_t *guest_cid = (volatile uint64_t *)dev->device_cfg;
    uint64_t cid = *guest_cid;

    if (cid == 0)
        TFAIL("guest_cid is 0 (reserved for hypervisor)");
    if (cid == 1)
        TFAIL("guest_cid is 1 (reserved for host)");
    if (cid == 2)
        TFAIL("guest_cid is 2 (reserved for host)");
    if (cid == 0xFFFFFFFF)
        TFAIL("guest_cid is VMADDR_CID_ANY");

    return TEST_PASS;
}

REGISTER_TEST(V0078, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_config_cid,
              "Read guest_cid config and verify valid CID",
              VIRTIO_SPEC_V1_2, "5.10.4");
