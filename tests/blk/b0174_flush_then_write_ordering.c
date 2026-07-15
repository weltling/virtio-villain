/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0174: flush_then_write_ordering
 *
 * Spec 5.2.6.2: A FLUSH makes all prior completed writes stable.
 * Submit WRITE sector 0, wait for completion, submit FLUSH, wait,
 * then submit WRITE sector 1, wait. Immediately read both sectors
 * back to verify the first write persisted through the flush and
 * the second write completed after it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_flush_ordering(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BLK_F_FLUSH)))
        return TEST_SKIP;

    /* Step 1: WRITE sector 0 with pattern 0xAA */
    struct virtio_blk_outhdr *h1 = vv_alloc_pages(1);
    uint8_t *d1 = vv_alloc_pages(1);
    uint8_t *s1 = vv_alloc_pages(1);
    h1->type = VIRTIO_BLK_T_OUT; h1->ioprio = 0; h1->sector = 0;
    memset(d1, 0xAA, 512); *s1 = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(h1), sizeof(*h1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(d1), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(s1), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*s1 != VIRTIO_BLK_S_OK) TFAIL("write0 status %u", *s1);

    /* Step 2: FLUSH */
    struct virtio_blk_outhdr *hf = vv_alloc_pages(1);
    uint8_t *sf = vv_alloc_pages(1);
    hf->type = VIRTIO_BLK_T_FLUSH; hf->ioprio = 0; hf->sector = 0;
    *sf = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hf), sizeof(*hf),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(sf), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*sf != VIRTIO_BLK_S_OK) TFAIL("flush status %u", *sf);

    /* Step 3: WRITE sector 1 with pattern 0xBB */
    struct virtio_blk_outhdr *h2 = vv_alloc_pages(1);
    uint8_t *d2 = vv_alloc_pages(1);
    uint8_t *s2 = vv_alloc_pages(1);
    h2->type = VIRTIO_BLK_T_OUT; h2->ioprio = 0; h2->sector = 1;
    memset(d2, 0xBB, 512); *s2 = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(h2), sizeof(*h2),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(d2), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(s2), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 2, 0);
    vring_raw_set_avail_idx(vr, 3);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*s2 != VIRTIO_BLK_S_OK) TFAIL("write1 status %u", *s2);

    /* Step 4: READ both sectors and verify patterns */
    struct virtio_blk_outhdr *hr = vv_alloc_pages(1);
    uint8_t *dr = vv_alloc_pages(1);
    uint8_t *sr = vv_alloc_pages(1);
    hr->type = VIRTIO_BLK_T_IN; hr->ioprio = 0; hr->sector = 0;
    memset(dr, 0, 512); *sr = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hr), sizeof(*hr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(dr), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(sr), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 3, 0);
    vring_raw_set_avail_idx(vr, 4);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*sr != VIRTIO_BLK_S_OK) TFAIL("read0 status %u", *sr);

    for (int i = 0; i < 512; i++) {
        if (dr[i] != 0xAA)
            TFAIL("sector0 byte %d: 0x%02x != 0xAA", i, dr[i]);
    }

    /* Read sector 1 */
    hr->sector = 1;
    memset(dr, 0, 512); *sr = 0xFF;
    vring_raw_set_avail(vr, 4, 0);
    vring_raw_set_avail_idx(vr, 5);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*sr != VIRTIO_BLK_S_OK) TFAIL("read1 status %u", *sr);

    for (int i = 0; i < 512; i++) {
        if (dr[i] != 0xBB)
            TFAIL("sector1 byte %d: 0x%02x != 0xBB", i, dr[i]);
    }
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0174, VIRTIO_PCI_DEVICE_BLK,
              test_blk_flush_ordering,
              "Write, flush, write: verify ordering and data persist",
              VIRTIO_SPEC_V1_2, "5.2.6.2",
              (1ULL << VIRTIO_BLK_F_FLUSH), 0);
