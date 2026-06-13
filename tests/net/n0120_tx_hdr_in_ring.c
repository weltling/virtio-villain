/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0120: Net TX header readable descriptor pointing at the ring.
 *
 * Spec 5.1.6: The TX header is a normal driver readable buffer.
 * Submit a TX whose header descriptor addr points back at the
 * driver's available ring rather than RAM. A device that reads
 * the header without verifying the region can interpret ring
 * indices as a virtio_net_hdr and act on garbage GSO and
 * checksum fields. The device must either reject the request
 * or treat the header content as opaque bytes.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_net_tx_hdr_in_ring(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *payload = vv_alloc_pages(1);
    memset(payload, 0xAB, 64);

    uint64_t avail_phys = vv_virt_to_phys((void *)vr->avail);

    vring_raw_set_desc(vr, 0, avail_phys, 12,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(payload), 64, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0120, VIRTIO_PCI_DEVICE_NET, test_net_tx_hdr_in_ring,
                "Net TX header descriptor pointing at the ring",
                VIRTIO_SPEC_V1_2, "5.1.6", 1);
