/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0127: contiguous data region whose base equals a wrapped boundary.
 *
 * Spec 2.7.5: addr and len define a guest physical buffer. When a device
 * folds a descriptor chain into one range by adding addr and len per
 * region, a first region that wraps past 2^64 leaves the wrapped end as
 * the running boundary. A second region whose base equals that wrapped
 * end is then treated as contiguous with an empty accumulated page list,
 * which a device may index without a guard.
 *
 * Mirror the fixed unit test: the first writable data region wraps so its
 * end lands at 0x100, and the second writable data region starts at 0x100
 * so it is GPA contiguous. The device must reject the chain or stay alive.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_contig_wrapped_region(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type   = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status     = 0xFF;

    /*
     * Chain:
     *   [0] header (readable)
     *   [1] writable data, addr = 2^64 - 256, len = 512 -> end wraps to
     *       0x100
     *   [2] writable data, addr = 0x100 -> contiguous with the wrapped
     *       end of region 1
     *   [3] status (writable)
     */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xFFFFFFFFFFFFFF00ULL, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, 0x100, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a contiguous wrapped region");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(T0127, VIRTIO_PCI_DEVICE_BLK, test_contig_wrapped_region,
              "Contiguous data region whose base equals a wrapped end",
              VIRTIO_SPEC_V1_2, "2.7.5");
