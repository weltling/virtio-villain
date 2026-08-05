/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0192: rx_bad_addr
 *
 * A receive descriptor is device writable but points at an address
 * far outside guest RAM. Any write attempt must fail safely rather
 * than pass an unchecked address into the host translation path.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_rx_bad_addr(struct virtio_dev *dev,
                                          struct vring *vr)
{
    vring_raw_set_desc(vr, 0, 0xFFFFFFFFFFFF0000ULL, 1526,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0192, VIRTIO_PCI_DEVICE_NET, test_net_rx_bad_addr,
                "Receive descriptor with an out of range address",
                VIRTIO_SPEC_V1_2, "2.7.5", 0);
