/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0168: Block request via INDIRECT descriptor pointing at the ring.
 *
 * Spec 2.7.7: An indirect descriptor table is a separate buffer
 * referenced from the ring; the spec does not forbid the table's
 * physical address from overlapping the descriptor area itself,
 * but a device that walks the resulting tables without bounding
 * the depth may loop on the overlap. Submit a request whose
 * single ring descriptor has VRING_DESC_F_INDIRECT set and whose
 * addr points back at the ring's own descriptor area. The device
 * must reject the malformed request or bound its walk; it must
 * not hang.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_blk_indirect_self_ref(struct virtio_dev *dev,
                                                struct vring *vr)
{
    uint64_t desc_phys = vv_virt_to_phys((void *)vr->desc);

    vring_raw_set_desc(vr, 0, desc_phys, 16 * sizeof(vr->desc[0]),
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0168, VIRTIO_PCI_DEVICE_BLK, test_blk_indirect_self_ref,
              "INDIRECT descriptor pointing at the ring",
              VIRTIO_SPEC_V1_2, "2.7.7");
