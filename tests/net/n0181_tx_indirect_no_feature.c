/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0181: tx_indirect_no_feature
 *
 * Set VRING_DESC_F_INDIRECT on a transmit descriptor without having
 * negotiated VIRTIO_F_INDIRECT_DESC. Spec 2.7.5.3 says the driver
 * MUST NOT set the flag unless the feature was negotiated, so the
 * device must refuse the request without harm to the host.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_indirect_no_feature(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);
    struct vring_desc *indirect = vv_alloc_pages(1);

    memset(hdr, 0, sizeof(*hdr));
    memset(frame, 0xAA, 512);

    indirect[0].addr = vv_virt_to_phys(hdr);
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 1;
    indirect[1].addr = vv_virt_to_phys(frame);
    indirect[1].len = 512;
    indirect[1].flags = 0;
    indirect[1].next = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(indirect),
                       2 * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0181, VIRTIO_PCI_DEVICE_NET, test_net_tx_indirect_no_feature,
                "Transmit indirect flag without the feature",
                VIRTIO_SPEC_V1_2, "2.7.5.3", 1);
