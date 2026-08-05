/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0199: tx_indirect_max_entries
 *
 * With VIRTIO_F_INDIRECT_DESC negotiated a transmit request uses a
 * full indirect table, a header entry followed by many readable
 * fragments. The device must gather the whole table without
 * overrunning it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

#define ENTRIES 16

static test_result_t test_net_tx_indirect_max_entries(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_INDIRECT_DESC))
        return TEST_SKIP;

    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frag = vv_alloc_pages(1);
    struct vring_desc *indirect = vv_alloc_pages(1);
    memset(hdr, 0, sizeof(*hdr));
    memset(frag, 0x5a, 128);

    indirect[0].addr = vv_virt_to_phys(hdr);
    indirect[0].len = sizeof(*hdr);
    indirect[0].flags = VRING_DESC_F_NEXT;
    indirect[0].next = 1;
    for (int i = 1; i < ENTRIES; i++) {
        indirect[i].addr = vv_virt_to_phys(frag + (i * 128 % 4096));
        indirect[i].len = 128;
        indirect[i].flags = i == ENTRIES - 1 ? 0 : VRING_DESC_F_NEXT;
        indirect[i].next = i == ENTRIES - 1 ? 0 : i + 1;
    }

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(indirect),
                       ENTRIES * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q_REQUIRES(N0199, VIRTIO_PCI_DEVICE_NET,
                         test_net_tx_indirect_max_entries,
                         "Transmit indirect table using a full entry set",
                         VIRTIO_SPEC_V1_2, "2.7.5.3", 1,
                         (1ULL << VIRTIO_F_INDIRECT_DESC), 0);
