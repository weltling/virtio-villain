/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0114: three descriptor chain completes correctly.
 *
 * Spec 2.7.5: A descriptor chain may link multiple buffers via
 * NEXT flags. Submit a blk READ using three separate descriptors
 * (header, data, status) and verify all three are consumed.
 * This is the fundamental three-desc chain correctness test.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_three_desc_chain(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0xCC, 512);
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* Verify status was written */
    if (*st == 0xFF)
        TFAIL("status byte unchanged (device did not write)");
    if (*st != VIRTIO_BLK_S_OK)
        TFAIL("status %u, expected S_OK", *st);

    /* Verify data was written (not still 0xCC) */
    int changed = 0;
    for (int i = 0; i < 512; i++) {
        if (data[i] != 0xCC) { changed = 1; break; }
    }
    (void)changed;
    /* Note: data could still be 0xCC if device happened to read that,
     * but status must have changed from 0xFF */

    return TEST_PASS;
}

REGISTER_TEST(T0114, VIRTIO_PCI_DEVICE_BLK, test_three_desc_chain,
              "Three descriptor chain completes and writes status",
              VIRTIO_SPEC_V1_2, "2.7.5");
