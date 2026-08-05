/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0198: tx_indirect_oob_next
 *
 * With VIRTIO_F_INDIRECT_DESC negotiated the first entry of a transmit
 * indirect table links to a next index beyond the entries the table
 * holds. The device must refuse the out of range link without harm.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_indirect_oob_next(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_INDIRECT_DESC))
        return TEST_SKIP;

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    struct vring_desc *indirect = vv_alloc_pages(1);
    memset(hdr, 0, sizeof(*hdr));

    indirect[0].addr = vv_virt_to_phys(hdr);
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 99;      /* beyond the single entry table */

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(indirect),
                       sizeof(struct vring_desc), VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q_REQUIRES(N0198, VIRTIO_PCI_DEVICE_NET,
                         test_net_tx_indirect_oob_next,
                         "Transmit indirect entry next out of bounds",
                         VIRTIO_SPEC_V1_2, "2.7.5.3.1", 1,
                         (1ULL << VIRTIO_F_INDIRECT_DESC), 0);
