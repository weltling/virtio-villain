/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0196: GET_ID response is NUL padded ASCII.
 *
 * Spec 5.2.6.2: GET_ID returns a NUL-padded ASCII string up to
 * 20 bytes. Verify the response contains only printable ASCII or
 * NUL bytes, and that after the first NUL all remaining bytes
 * are also NUL.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_get_id_format(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *id = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_GET_ID;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(id, 0xFE, 20);
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(id), 20,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;
    if (*st != VIRTIO_BLK_S_OK) TFAIL("status %u", *st);

    /* Verify format: printable ASCII or NUL, NUL-padded after first NUL */
    int past_nul = 0;
    for (int i = 0; i < 20; i++) {
        if (id[i] == 0) { past_nul = 1; continue; }
        if (past_nul)
            TFAIL("non-NUL byte 0x%02x at offset %d after NUL", id[i], i);
        if (id[i] < 0x20 || id[i] > 0x7E)
            TFAIL("non-printable byte 0x%02x at offset %d", id[i], i);
    }

    return TEST_PASS;
}

REGISTER_TEST(B0196, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id_format,
              "GET_ID response is NUL padded printable ASCII",
              VIRTIO_SPEC_V1_2, "5.2.6.2");
