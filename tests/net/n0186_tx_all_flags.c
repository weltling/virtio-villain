/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0186: tx_all_flags
 *
 * A transmit descriptor sets NEXT, WRITE and INDIRECT together, a
 * contradictory combination. The device must handle the conflicting
 * flags gracefully rather than fault the host.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_all_flags(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    memset(hdr, 0, sizeof(*hdr));

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE |
                           VRING_DESC_F_INDIRECT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(hdr), sizeof(*hdr), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0186, VIRTIO_PCI_DEVICE_NET, test_net_tx_all_flags,
                "Transmit descriptor with contradictory flags",
                VIRTIO_SPEC_V1_2, "2.7.5", 1);
