/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0096: net_tx_uso6_no_feature
 *
 * Submit a TX packet with USOv6 gso_type (6) without negotiating
 * GUEST_USO6 or HOST_USO. Spec v1.3 5.1.6.2: the device must
 * reject USO frames when the feature is not negotiated.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>


static test_result_t test_net_tx_uso6_no_feature(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    uint8_t *pkt = vv_alloc_pages(1);
    struct virtio_net_hdr_mrg *h = (void *)pkt;

    h->flags = 0;
    h->gso_type = VIRTIO_NET_HDR_GSO_UDP_L4;
    h->hdr_len = 62;  /* IPv6 + UDP */
    h->gso_size = 1400;
    h->csum_start = 0;
    h->csum_offset = 0;
    h->num_buffers = 0;

    memset(pkt + sizeof(*h), 0x66, 60);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pkt),
                       sizeof(*h) + 60, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0096, VIRTIO_PCI_DEVICE_NET, test_net_tx_uso6_no_feature,
                "TX with USOv6 gso_type without USO feature negotiated",
                VIRTIO_SPEC_V1_3, "5.1.6.2", 1);
