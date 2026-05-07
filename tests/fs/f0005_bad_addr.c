/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0005: virtio-fs request with out of range address.
 *
 * Submit a request descriptor whose addr points to
 * 0xFFFFFFFFFFFF0000, outside any valid guest memory region. The
 * device must fail the read attempt safely rather than pass an
 * unchecked address into the host's translation path.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_fs_bad_addr(struct virtio_dev *dev,
                                      struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    uint64_t resp_phys = vv_virt_to_phys(page);

    vring_raw_set_desc(vr, 0, 0xFFFFFFFFFFFF0000ULL, 64,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, resp_phys, 256, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(F0005, VIRTIO_PCI_DEVICE_FS, test_fs_bad_addr,
                "FS request with out of range address",
                VIRTIO_SPEC_V1_2, "5.11.6", 1);
