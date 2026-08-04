/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0180: net tx readable frame descriptor addr plus len wraps 2^64.
 *
 * Sibling to N0001, which sends an oversized frame with the end below
 * 2^64. Here the frame descriptor base sits near the top of the address
 * space and the length makes addr plus len wrap to a low value. The
 * frame descriptor is device readable, so this exercises the device read
 * from guest memory on the transmit path rather than the receive write
 * path of N0179. The device must not read outside the guest mapping or
 * crash the VMM. Completing, silently rejecting, or wedging the queue are
 * all acceptable.
 *
 * Spec 5.1.6.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_net_tx_addr_len_wrap(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    memset(hdr, 0, sizeof(*hdr));
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xFFFFFFFFFFFFF000ULL, 0x2000, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0180, VIRTIO_PCI_DEVICE_NET, test_net_tx_addr_len_wrap,
                "Net tx readable frame addr plus len wraps 64 bits",
                VIRTIO_SPEC_V1_2, "5.1.6", 1);
