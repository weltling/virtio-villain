/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0071: kick_empty_avail_eq_used
 *
 * Kick when avail idx equals used idx (ring appears empty from both
 * perspectives). The device should detect nothing new is available
 * and not process stale descriptors from a previous iteration.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_kick_empty_avail_eq_used(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* Set up a valid request in descriptor slots */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    /*
     * Set avail idx = 5, and pretend used has already consumed up to 5.
     * Then kick - the device should see nothing new.
     */
    vring_raw_set_avail_idx(vr, 5);
    vr->used->idx = 5;
    __sync_synchronize();

    /* Leave avail ring entries pointing at desc 0, but don't advance idx */
    return vv_kick_expect_reject(dev, vr, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0071, VIRTIO_PCI_DEVICE_BLK, test_kick_empty_avail_eq_used,
              "Kick with avail idx == used idx (empty ring confusion)",
              VIRTIO_SPEC_V1_2, "2.7.7");
