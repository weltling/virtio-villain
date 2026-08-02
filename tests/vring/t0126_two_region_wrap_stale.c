/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0126: two data regions where the first wraps and leaves stale state.
 *
 * Spec 2.7.5: addr and len define a guest physical buffer. A device that
 * builds a combined range for a descriptor chain by adding addr and len
 * per region can be fooled when the first region wraps past 2^64. The
 * wrapped end is carried into the next region as the running boundary,
 * so a well formed second region is folded onto a bogus base. The end
 * range then covers fewer pages than the claimed total length.
 *
 * Submit a read whose first writable data descriptor wraps and whose
 * second writable data descriptor is well formed. The device must reject
 * the chain or stay alive. A wrapping region followed by a valid one also
 * exercises the stale running boundary left by the first region.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_two_region_wrap_stale(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    uint64_t good_phys;
    uint8_t *good = vv_alloc_page_high(&good_phys);
    (void)good;

    hdr->type   = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status     = 0xFF;

    /*
     * Chain:
     *   [0] header (readable)
     *   [1] writable data, addr + len wraps past 2^64
     *   [2] writable data, well formed
     *   [3] status (writable)
     *
     * desc[1].addr = 0xFFFFFFFF_FFFFF000, len = 0x2000 -> end wraps to
     * 0x1000. The running boundary carried into desc[2] is the wrapped
     * value, so the second region is folded onto a bogus base.
     */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, 0xFFFFFFFFFFFFF000ULL, 0x2000,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, good_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a wrapping region chain");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(T0126, VIRTIO_PCI_DEVICE_BLK, test_two_region_wrap_stale,
              "Two data regions, first wraps and leaves stale boundary",
              VIRTIO_SPEC_V1_2, "2.7.5");
