/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0114: Net TX with truncated virtio_net_hdr.
 *
 * Spec 5.1.6: Submit a transmit request where the header descriptor
 * is only 4 bytes (less than sizeof(virtio_net_hdr)). The device
 * must not read past the descriptor boundary.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_truncated_hdr(struct virtio_dev *dev,
                                               struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    /* Only 4 bytes of header (truncated) then data */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 4,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(buf) + 64, 64, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0114, VIRTIO_PCI_DEVICE_NET, test_net_tx_truncated_hdr,
                "Net TX with truncated header descriptor",
                VIRTIO_SPEC_V1_2, "5.1.6", 1);
