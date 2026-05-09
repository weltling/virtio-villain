/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0100: vring_two_indirects_via_next
 *
 * Build two indirect tables and reference them from two main
 * descriptors chained with NEXT. Spec 2.7.5.3 forbids INDIRECT
 * combined with NEXT in the main descriptor. The device must
 * reject the chain.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vring_two_indirects(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct vring_desc *ind1 = vv_alloc_pages(1);
    struct vring_desc *ind2 = vv_alloc_pages(1);
    uint8_t *buf = vv_alloc_pages(1);

    ind1[0].addr = vv_virt_to_phys(buf);
    ind1[0].len = 1;
    ind1[0].flags = VRING_DESC_F_WRITE;
    ind1[0].next = 0;

    ind2[0].addr = vv_virt_to_phys(buf);
    ind2[0].len = 1;
    ind2[0].flags = VRING_DESC_F_WRITE;
    ind2[0].next = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(ind1),
                       sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT | VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(ind2),
                       sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0100, VIRTIO_PCI_DEVICE_BLK, test_vring_two_indirects,
              "Two indirect tables linked via NEXT",
              VIRTIO_SPEC_V1_2, "2.7.5.3");
