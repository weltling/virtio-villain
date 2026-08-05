/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0197: tx_indirect_unaligned
 *
 * With VIRTIO_F_INDIRECT_DESC negotiated a transmit descriptor points
 * at an indirect table whose length is not a multiple of the
 * descriptor size. Spec 2.7.5.3.1 says the length must be a multiple
 * of the descriptor size, so the device must refuse it without harm.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_indirect_unaligned(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_INDIRECT_DESC))
        return TEST_SKIP;

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    struct vring_desc *indirect = vv_alloc_pages(1);
    memset(hdr, 0, sizeof(*hdr));

    indirect[0].addr = vv_virt_to_phys(hdr);
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = 0;
    indirect[0].next = 0;

    /* Length 17 is not a multiple of sizeof(struct vring_desc). */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(indirect), 17,
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q_REQUIRES(N0197, VIRTIO_PCI_DEVICE_NET,
                         test_net_tx_indirect_unaligned,
                         "Transmit indirect table length unaligned",
                         VIRTIO_SPEC_V1_2, "2.7.5.3.1", 1,
                         (1ULL << VIRTIO_F_INDIRECT_DESC), 0);
