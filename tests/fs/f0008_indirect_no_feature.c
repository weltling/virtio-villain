/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0008: virtio-fs indirect without negotiated feature.
 *
 * Submit a request descriptor with VRING_DESC_F_INDIRECT set. The
 * harness negotiates zero features so VIRTIO_F_INDIRECT_DESC is
 * not active. Per spec 2.7.7 the driver MUST NOT use indirect
 * descriptors without the feature. The device must reject.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_fs_indirect_no_feature(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct vring_desc *itab = vv_alloc_pages(1);
    uint64_t itab_phys = vv_virt_to_phys(itab);
    uint64_t payload_phys = itab_phys + 256;

    itab[0].addr  = payload_phys;
    itab[0].len   = 64;
    itab[0].flags = VRING_DESC_F_NEXT;
    itab[0].next  = 1;
    itab[1].addr  = payload_phys + 256;
    itab[1].len   = 256;
    itab[1].flags = VRING_DESC_F_WRITE;
    itab[1].next  = 0;

    vring_raw_set_desc(vr, 0, itab_phys, sizeof(*itab) * 2,
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0008, VIRTIO_PCI_DEVICE_FS, test_fs_indirect_no_feature,
                "FS indirect without feature",
                VIRTIO_SPEC_V1_2, "2.7.7", 1);
