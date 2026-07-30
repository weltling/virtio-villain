/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0124: desc_addr_own_used_ring
 *
 * Point a read request's data descriptor at the queue's own used ring in
 * guest RAM. The device writes disk bytes over the used ring at the same
 * time it updates the used ring to signal completion. The device must
 * not corrupt its own completion bookkeeping or the host in the process.
 * A hang or a sanitizer fault is a guest triggered host problem. The
 * device outcome is up to it as long as the host survives and the next
 * test can still run.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_desc_addr_own_used_ring(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    /* Data descriptor overlaps the queue's own used ring. */
    vring_raw_set_desc(vr, 1, vr->used_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure writing into its own used ring");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(T0124, VIRTIO_PCI_DEVICE_BLK, test_desc_addr_own_used_ring,
              "Descriptor address overlapping the queue's own used ring",
              VIRTIO_SPEC_V1_2, "2.7.5");
