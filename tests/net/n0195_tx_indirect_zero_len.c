/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0195: tx_indirect_zero_len
 *
 * With VIRTIO_F_INDIRECT_DESC negotiated a transmit descriptor sets
 * the indirect flag but gives the table a zero length, so it holds no
 * entries. The device must handle the empty table without harm.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_indirect_zero_len(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_INDIRECT_DESC))
        return TEST_SKIP;

    struct vring_desc *indirect = vv_alloc_pages(1);
    memset(indirect, 0, sizeof(*indirect));

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(indirect), 0,
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q_REQUIRES(N0195, VIRTIO_PCI_DEVICE_NET,
                         test_net_tx_indirect_zero_len,
                         "Transmit indirect table with zero length",
                         VIRTIO_SPEC_V1_2, "2.7.5.3", 1,
                         (1ULL << VIRTIO_F_INDIRECT_DESC), 0);
