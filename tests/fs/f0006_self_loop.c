/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0006: virtio-fs chain with self loop.
 *
 * Build a request chain whose head sets VRING_DESC_F_NEXT and
 * points back at itself. The device must detect the cycle rather
 * than walk the chain forever.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_fs_self_loop(struct virtio_dev *dev,
                                       struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 64,
                       VRING_DESC_F_NEXT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0006, VIRTIO_PCI_DEVICE_FS, test_fs_self_loop,
                "FS chain self loop",
                VIRTIO_SPEC_V1_2, "2.7.5", 1);
